# Camera Acquisition & Frame Pipeline

## 1. Why this subsystem matters

The capture pipeline is the product's trust boundary. If frames are misinterpreted, silently dropped, reordered or altered, every downstream feature becomes irrelevant.

## 2. Device discovery

Define two concepts:

- **Backend**: a technology/provider capable of enumerating devices, e.g. UVC, ZWO SDK, QHY SDK.
- **Device**: one physical/logical camera instance.

A backend interface should conceptually support:

```cpp
class ICameraBackend {
public:
    virtual std::string id() const = 0;
    virtual std::vector<CameraDescriptor> enumerate() = 0;
    virtual std::unique_ptr<ICameraDevice> open(const CameraId&) = 0;
};
```

Exact signature may evolve.

Camera identity should include a stable identifier where the OS/SDK provides one. Never assume model name is unique.

## 3. Capabilities

Controls are capability-driven.

A generic scalar control descriptor might carry:

```text
id: Exposure
value_type: duration
unit: microseconds
min/max/step
default
supports_auto
mutable_while_streaming
```

Enumerated controls use allowed values rather than meaningless numeric ranges.

The application may define canonical semantics for common controls while preserving an advanced vendor-specific escape hatch later.

## 4. Stream modes

A mode describes more than resolution:

```text
width × height
pixel format
bit depth
packing
nominal/supported frame-rate range
binning if hardware-level
ROI constraints
USB/bandwidth mode where exposed
```

Do not label a 12-bit packed stream simply as “RAW12” without specifying memory layout internally.

## 5. Pixel formats

Project-level `PixelFormat` must be explicit enough to avoid ambiguous conversions.

Example families:

- Mono8;
- Mono16;
- Bayer8(pattern);
- Bayer16(pattern, validBits);
- PackedBayer10/12/...;
- RGB24;
- BGR24;
- YUY2 or other UVC formats if used;
- unknown/vendor-specific represented safely until decoded.

Bayer pattern is metadata, not inferred every frame.

## 6. NexImage initial investigation

For the initial Celestron camera:

1. enumerate available USB/video interfaces on macOS;
2. identify whether macOS presents it through a standard video class/backend or requires a special path;
3. enumerate actual negotiated formats;
4. capture raw byte samples without debayering;
5. verify dimensions/stride/frame size;
6. render grayscale raw mosaic to inspect Bayer structure;
7. test RGGB/BGGR/GRBG/GBRG patterns against a known colored scene;
8. compare with Windows output where useful;
9. document the discovered transport and control behavior in a device compatibility note.

Do not hardcode an assumed Bayer pattern based solely on a web page. Validate with captured data.

## 7. Acquisition worker

Requirements:

- starts/stops deterministically;
- does minimal work in SDK callback/capture loop;
- timestamps frame arrival with a monotonic host clock in addition to camera timestamps when available;
- increments sequence counter;
- validates expected payload size;
- moves/copies frame to owned pool buffer;
- publishes to downstream transport;
- records acquisition errors with vendor diagnostics.

Do not debayer inside a vendor callback unless the SDK itself provides the only practical decoded path and raw access is unavailable.

## 8. Bounded transport

The pipeline must never grow memory without limit.

Example policies:

### Preview queue

Capacity small (often effectively latest-frame semantics).

If preview is slower than acquisition, discard stale preview frames and show the freshest available frame.

### Recording queue

Capacity sized to absorb short storage jitter but not seconds/minutes of unbounded frames.

If near saturation:

- report warning;
- record queue metrics;
- if overflow occurs, increment dropped-recording-frame count and surface it prominently;
- never silently block acquisition forever.

Exact capacities are tuned by benchmarks, not fixed in architecture docs.

## 9. Debayer pipeline

Debayering is context-sensitive.

For preview:

- favor speed and visually accurate color;
- allow algorithm choice later if valuable;
- apply color transforms after raw conversion.

For scientific processing:

- preserve raw capture;
- perform high-quality debayer later if workflow requires it.

Tests must include synthetic Bayer mosaics whose expected color is mathematically known.

## 10. Bit-depth handling

Do not collapse >8-bit camera data to 8-bit before recording merely because the display uses 8-bit output.

Maintain a distinction:

```text
camera representation -> canonical high-bit-depth/raw frame
                        ├-> recorder
                        └-> preview tone mapping -> display
```

## 11. Histogram

Histogram computation should specify:

- raw or display-domain histogram;
- mono or per-channel;
- binning strategy by bit depth;
- sampled frame frequency;
- clipping percentage calculation.

UI should communicate what domain is displayed.

## 12. White balance and color

Separate:

- camera hardware white-balance controls;
- application preview color balance;
- post-processing color balance.

Do not bake preview white balance into raw recorded frames unless the selected recording format/workflow explicitly requests processed RGB.

## 13. Recording

Recorder input contract should include:

- frame descriptor;
- raw frame bytes/typed buffer;
- timestamp;
- sequence number;
- snapshot of relevant capture metadata;
- session identifier.

Recorder lifecycle:

1. prepare path and check writable location;
2. open file;
3. write provisional header if format requires;
4. stream frames;
5. flush strategically;
6. finalize metadata/header;
7. fsync only where justified by data-safety/performance tradeoff;
8. atomically mark session complete in metadata.

Partial/crashed captures should be recoverable where the format allows.

## 14. Disk health

Before/during recording, monitor enough to give actionable feedback:

- estimated stream throughput;
- actual write throughput;
- queue occupancy;
- free disk space;
- write errors.

Do not claim exact remaining recording time if frame compression/format makes it highly variable; label estimates.

## 15. Session metadata

Use a sidecar metadata file if the capture format cannot represent all information cleanly. Keep it versioned.

Potential structure:

```json
{
  "schemaVersion": 1,
  "appVersion": "...",
  "sessionId": "...",
  "camera": {},
  "stream": {},
  "controls": {},
  "capture": {},
  "health": {}
}
```

## 16. Multi-camera future

Do not assume a global singleton camera.

The application may initially support one active camera session, but domain identifiers and ownership should permit multiple devices later.

## 17. Hardware test matrix

For each supported device maintain:

- OS and architecture;
- camera firmware if visible;
- backend/SDK version;
- supported resolutions/formats;
- exposure/gain behavior;
- color pattern;
- max tested stable throughput;
- unplug/reconnect behavior;
- known issues;
- date/tester/app build.
