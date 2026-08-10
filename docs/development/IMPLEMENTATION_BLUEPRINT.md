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

`IRecorder` and a first implementation now exist (`core/recording`,
`astro::recording::RawSequenceRecorder`), but the implementation is a
documented interim placeholder format (magic `ACRAWSEQ`), not SER. A real
`SerRecorder` should implement the same `IRecorder` interface once the
groundwork above is done.

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
