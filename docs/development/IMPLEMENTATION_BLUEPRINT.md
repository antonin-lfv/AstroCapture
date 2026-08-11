# Implementation Blueprint

This document translates the architecture into an initial implementation plan. Code examples are illustrative contracts, not frozen APIs.

## 1. Initial CMake target graph

A clean first target graph can be:

```text
astro_domain          pure/core types and state machines
astro_processing      debayer/histogram/basic image operations
astro_recording       recording interfaces + SER
astro_camera          camera contracts + manager
astro_driver_mock     simulated/replay camera
astro_driver_<name>   vendor/platform backends
astro_application     orchestration / use cases
astro_ui_models       QObject/QML-facing presentation models
AstroCapture          final Qt executable
```

Dependency direction:

```text
AstroCapture
  -> astro_ui_models
      -> astro_application
          -> astro_camera / astro_recording / astro_processing
              -> astro_domain

astro_driver_* -> astro_camera + astro_domain
```

A vendor backend should not force the entire application to include its SDK headers.

## 2. Suggested CMake options

Keep options semantic and limited:

```cmake
option(ASTRO_BUILD_TESTS "Build tests" ON)
option(ASTRO_ENABLE_DEVTOOLS "Enable developer tools" OFF)
option(ASTRO_ENABLE_MOCK_CAMERA "Build mock camera backend" ON)
option(ASTRO_ENABLE_ONNX "Enable production ML inference" OFF)
option(ASTRO_ENABLE_DRIVER_NEXIMAGE "Build initial NexImage backend" ON)
option(ASTRO_ENABLE_DRIVER_ZWO "Build ZWO backend" OFF)
```

Production presets explicitly set approved values. Do not infer “production” from `NDEBUG` alone.

## 3. Build identity

Generate a header/resource containing:

```text
ASTRO_VERSION
ASTRO_GIT_SHA
ASTRO_BUILD_CHANNEL
ASTRO_BUILD_TYPE
ASTRO_DEVTOOLS_ENABLED
```

Display full values in About/Diagnostics, not necessarily in the main UI.

## 4. Domain types

Illustrative:

```cpp
enum class BayerPattern { None, RGGB, BGGR, GRBG, GBRG };

enum class PixelStorage {
    Mono8,
    Mono16,
    Bayer8,
    Bayer16,
    RGB24,
    BGR24,
    Unknown
};

struct FrameDescriptor {
    uint32_t width{};
    uint32_t height{};
    uint32_t strideBytes{};
    PixelStorage storage{PixelStorage::Unknown};
    BayerPattern bayer{BayerPattern::None};
    uint8_t validBits{};
};

struct FrameMetadata {
    uint64_t sequence{};
    std::chrono::steady_clock::time_point hostArrival;
    std::optional<uint64_t> cameraTimestamp;
};
```

Packed 10/12-bit formats may require richer storage descriptors. Do not force them into `Mono16` without recording whether unpacking occurred.

## 5. Frame ownership

Prefer an immutable consumer view with shared lifetime:

```cpp
struct FrameBuffer {
    FrameDescriptor descriptor;
    FrameMetadata metadata;
    std::shared_ptr<const BufferStorage> storage;
};
```

The real implementation may use intrusive/ref-counted pool handles for lower overhead after profiling.

The key invariant is: a consumer never references memory after the SDK or pool has reclaimed it.

## 6. Camera contracts

Illustrative capability model:

```cpp
using CameraId = std::string;

struct CameraDescriptor {
    CameraId id;
    std::string displayName;
    std::string backendId;
    std::optional<std::string> serial;
};

class ICameraDevice {
public:
    virtual ~ICameraDevice() = default;
    virtual CameraDescriptor descriptor() const = 0;
    virtual std::vector<StreamMode> streamModes() const = 0;
    virtual CameraCapabilities capabilities() const = 0;
    virtual Result<void> configure(const StreamConfiguration&) = 0;
    virtual Result<void> start(FrameSink&) = 0;
    virtual Result<void> stop() = 0;
    virtual Result<ControlValue> getControl(ControlId) = 0;
    virtual Result<void> setControl(ControlId, const ControlValue&) = 0;
};
```

Consider whether `start(FrameSink&)` callbacks or a pull API better fits actual SDKs. Hide the choice from upper layers.

## 7. Camera manager

Responsibilities:

- register enabled backends;
- enumerate/merge devices;
- avoid duplicate presentation if the same device appears through multiple backends, or clearly expose backend choice;
- open selected device;
- monitor discovery changes where supported;
- expose support/diagnostic information.

Do not let `CameraManager` become capture-session logic.

## 8. Capture session service

A session coordinates one selected camera and downstream consumers.

Conceptual API:

```cpp
class CaptureSession {
public:
    Result<void> attach(std::unique_ptr<ICameraDevice> camera);
    Result<void> startPreview(const StreamConfiguration&);
    Result<void> startRecording(const RecordingRequest&);
    Result<void> stopRecording();
    Result<void> stopPreview();
    CaptureSnapshot snapshot() const;
};
```

Recording may be started while preview stream already runs; avoid unnecessary camera stream restart unless mode changes require it.

## 9. Frame fan-out

Do not call all consumers inline from acquisition callback.

A possible implementation:

```text
Acquisition
  ↓
FrameHub
  ├─ LatestFrameSlot -> PreviewWorker
  ├─ BoundedQueue    -> RecorderWorker
  └─ SampledQueue    -> AnalysisWorker
```

### LatestFrameSlot

Stores/replaces the most recent preview frame. The display consumes latest available frame at its pace.

### Recorder queue

FIFO ordering; bounded; overflow is recorded as integrity failure.

### Analysis queue

May drop stale work freely according to feature policy.

## 10. Preview model

`LiveViewModel` should expose state/texture integration, not raw camera implementation.

The QML tree should not receive a new huge `QImage` property every 1/120 second if a more efficient render bridge is available. Start with correctness, then profile and move to a custom `QQuickItem`/texture path if necessary.

## 11. UI presentation models

Examples:

```text
ApplicationViewModel
CameraListModel
CameraControlModel
CaptureViewModel
HistogramViewModel
DiagnosticsViewModel (dev/advanced)
```

`CameraControlModel` can dynamically represent device capabilities while semantic common controls receive optimized UI components.

## 12. Startup sequence

```text
main()
  -> initialize logging
  -> load app/build identity
  -> load/migrate settings
  -> create services
  -> register compiled camera backends
  -> initialize Qt/QML engine
  -> expose root presentation model(s)
  -> asynchronously enumerate devices
  -> show main workspace immediately
```

Do not block initial UI for slow SDK device enumeration if it can be asynchronous.

## 13. Shutdown sequence

Order matters:

1. prevent new capture commands;
2. stop/finalize recording;
3. stop camera streams;
4. drain/cancel processing jobs;
5. release devices/vendor SDK resources;
6. persist safe settings/workspace state;
7. stop logging/background services.

If user closes during recording, prefer a lightweight explicit choice if immediate closure risks corrupting the active file.

## 14. DEV mode implementation

Development mode should be identifiable through build channel and feature availability rather than scattered `#ifdef DEBUG`.

Possible:

```cpp
struct BuildFeatures {
    bool devTools;
    bool mockCamera;
    bool experimentalAi;
};
```

Compile out sensitive/unwanted tooling from final release where appropriate.

## 15. CLI development switches

Useful non-public/advanced options:

```text
--log-level
--log-category
--camera mock
--mock-source
--replay <file>
--safe-mode
--disable-gpu-preview
--disable-ai
--reset-dev-settings
```

CLI must never be the only way normal users can perform essential configuration.

## 16. Settings namespaces

Suggested conceptual keys:

```text
application/
workspace/
recording/
cameraProfiles/<stable-camera-key>/
captureProfiles/<uuid>/
diagnostics/
```

Use a schema version and migrations.

## 17. File/session naming

Do not hardwire one naming convention. Provide a default pattern and later configurable tokens such as:

```text
{date}_{time}_{target}_{camera}_{sequence}
```

Sanitize for all target filesystems.

## 18. SER implementation plan

Before implementation:

- obtain authoritative format specification/reference implementation;
- define supported color IDs/bit depths/endian behavior;
- create golden files;
- test round-trip or validation with established planetary software;
- verify timestamps/trailer behavior if used.

Create `IRecorder` first so file-format details do not leak into capture orchestration.

`IRecorder` and its first implementation (`core/recording`,
`astro::recording::RawSequenceRecorder`, the documented interim `ACRAWSEQ`
placeholder format) both exist, and now so does `astro::recording::SerRecorder`
implementing the same interface for real SER — see section 22.9 for what
"before implementation" above ended up meaning in practice: this project has
no Windows/AutoStakkert/PIPP/Registax available to cross-validate against, so
implementation went ahead against the SER specification directly rather than
staying blocked on that. SER is now the default (Settings' "Video format" toggle); `RawSequenceRecorder`
remains selectable there too, both as a fallback for formats SER has no
color ID for and so `.acraw` files from earlier in this project's history
stay explicable.

## 19. First NexImage spike

Create a small `camera_probe` tool separate from the polished UI that can:

- list matching devices;
- list modes;
- start N frames;
- write raw payload sample;
- print frame size/stride/timestamps;
- optionally save a debayered PNG preview;
- cycle Bayer patterns for comparison;
- dump supported controls.

The probe is allowed to be ugly. It exists to reduce uncertainty before integrating product UI.

## 20. Test-first Bayer investigation

Create synthetic image with known large red/green/blue patches, mosaic it into all four Bayer patterns, then ensure the production debayer wrapper restores expected dominant colors within tolerance.

This catches the exact class of bug that motivated AstroCapture.

## 21. Packaging boundary

The executable should be runnable from the build directory in DEV. Packaging is a separate build/install stage:

```text
compile/link -> local runnable executable
                    ↓
                test/smoke
                    ↓
              deploy dependencies
                    ↓
             sign/package/notarize
```

Never make package creation a prerequisite for normal local debugging.

## 22. Gate D feature-parity backlog

Plan behind ROADMAP.md's Gate D "Feature-parity backlog", in the same
priority order. Each item is scoped to be implementable and verifiable
against the mock camera alone.

### 22.1 ROI/crop capture

- Add `Roi` (x, y, width, height) to `StreamConfiguration` alongside the
  existing mode/fps fields (section 6); a backend that ignores it (mock,
  initially) just returns the full frame.
- Mock backend: honor it by cropping its generated Bayer pattern, so the
  behavior is actually exercised in tests, not just plumbed through.
- UI: a draggable/resizable rectangle over the live preview (own QML item,
  reuses LivePreviewItem's frame-to-widget coordinate mapping), plus a
  reset-to-full-frame action.
- Changing ROI while connected re-runs `configure()`; treat it like any
  other stream reconfiguration, not a special case.

### 22.2 Preview overlays

- Zoom/loupe: a small magnified inset following the cursor, sampling
  `frame()`'s QImage directly (render/GUI-thread only, same contract
  `frame()` already documents) — no new frame path needed.
- Center crosshair: static QML overlay, no controller changes.
- Clipping indicators: extend `Histogram` (astro_processing) with clipped
  low/high pixel counts (already has the per-bin data, this is a
  reduction over the existing bins) and expose via `HistogramItem`, then
  render as colored highlights at the histogram's edges.

### 22.3 Additional camera controls

- Extend `ControlId` (CameraControl.hpp) with `Gamma`, `WhiteBalanceRed`,
  `WhiteBalanceBlue`, `Brightness`/`Offset`, `FrameRateLimit`.
- Follow the existing Exposure/Gain pattern exactly: capability-driven
  (`*Supported` property, hidden control when absent), typed
  `Q_PROPERTY` pairs on `PreviewController`, persisted the same way as
  exposure/gain in `Settings`. Do not build a generic control-list model
  for this (blueprint section 11 already flags that as the point to
  generalize once a *third* control needs exposing — this adds five, so
  it's worth reconsidering then, not preemptively now).

### 22.4 Advanced capture statistics

- Min/max/mean: compute alongside the existing histogram pass in
  `astro::processing` (single extra reduction over pixels already being
  visited, avoid a second full-frame scan) and expose next to the current
  `fps`/`previewSkippedFrames` diagnostics.
- Disk-write fps vs. capture fps: the writer thread already timestamps
  each write for `recordingElapsedSeconds()`; derive a rolling write-fps
  from that and compare against the acquisition-side `fps()`. Surfacing
  the two side by side in the Recording sidebar section explains
  `recordingDroppedFrames_` increases instead of just counting them.

### 22.5 Real debayer for preview

- Replace `FramePreviewImage`'s 2x2 nearest-neighbor Bayer8 path with
  bilinear interpolation. Keep it in `astro_processing`, not inline in
  the Qt-facing wrapper, so section 20's synthetic-patch Bayer test can
  cover it directly instead of only through the UI.
- Nearest-neighbor was always documented as a placeholder (see
  FramePreviewImage.hpp); this closes that out rather than introducing
  new scope.

### 22.6 Per-frame timestamps in recordings

- `FrameDescriptor`/`FrameBuffer` already carry a capture timestamp
  (used for fps calculation) — thread it into `IRecorder::writeFrame()`
  and `RawSequenceRecorder`'s per-frame record, bumping the `ACRAWSEQ`
  format version.
- `RawSequenceReader` needs the matching read-side change; update its
  round-trip test (core/recording) to assert the timestamp survives.
- Design the on-disk field so a future `SerRecorder` (section 18) can
  populate SER's own trailer timestamp block from the same source data.

### 22.7 Keyboard shortcuts

- QML `Shortcut` items in Main.qml bound to the same invokables the
  sidebar buttons already call (`captureStillImage`, `startRecording`/
  `stopRecording`) — no controller changes, this is UI-only wiring.
- Disable/no-op consistently with the buttons' own enabled state (e.g.
  don't let a shortcut start a second recording).

### 22.8 Additional still-image save formats

- `captureStillImage()` currently hardcodes PNG. Introduce a save-format
  setting (Settings.hpp, same per-app scope as saveDirectory) with PNG/
  TIFF16/FITS options.
- TIFF16 needs the pre-debayer or 16-bit-if-available pixel data, not the
  already-8-bit-converted preview QImage — check what precision the
  backend actually delivers before promising 16-bit output.
- FITS is a plain flat binary format with a text header; a minimal writer
  (single HDU, no WCS) is a small, self-contained addition — no
  third-party dependency needed for this scope.

### 22.9 SER recording format — done

Implemented: `astro::recording::SerRecorder` (`core/recording`), against the
SER specification (178-byte header, ColorID/PixelDepthPerPlane/dimensions,
raw un-demosaiced frame data, optional per-frame UTC timestamp trailer using
the same steady-clock/wall-clock anchor technique section 22.6 built for
`RawSequenceReader`). Selectable as the recording format in Settings
("Video format": SER or ACRAW), SER is now the default.

What section 18's "before implementation" list actually became, since this
environment has no Windows/AutoStakkert/PIPP/Registax to get golden files
from or cross-validate against:

- specification: the public SER spec, unchanged and identically implemented
  by every tool named above for over a decade;
- color IDs/bit depths/endian: implemented for MONO/BAYER(all 4
  patterns)/RGB24/BGR24 at 8 or 16 bits; BGRA32 has no SER equivalent (no
  alpha channel), rejected at `open()` rather than silently dropping the
  channel;
- validation performed instead of golden-file cross-validation: real files
  recorded through the actual app (mock camera → acquisition → recording
  pipeline, not just the unit test) were independently re-parsed byte-by-
  byte against the spec in Python (not reusing any of this project's own
  parsing code), and separately recognized correctly by `file`(1)'s own SER
  magic support (`SER video sequence, bayer: RGGB, little-endian, width:
  640, height: 480, 8 bit, frames: N`) — a second, fully independent
  implementation agreeing on every header field;
- still open: an actual planetary-stacking tool (AutoStakkert/PIPP/
  Registax) has not opened one of these files. That needs Windows (or a
  compatible build of one of those tools) this environment doesn't have —
  flagged here rather than silently dropped.
