# ADR-007 — ZWO ASI camera backend (`drivers/zwo`)

## Status
Accepted, with an explicit unverified-on-hardware caveat (see Consequences).

## Context

ZWO ASI is by far the most common camera brand among amateur astronomers — broader than this, having a driver only for one old TIS/Celestron camera family (ADR-006) makes the addressable user base for this app small regardless of anything else built. ZWO publishes an official SDK, `ASICamera2`, that is:

- documented with a clear, synchronous, polling-based C API (enumerate → open → init → configure ROI/format → start capture → loop `ASIGetVideoData` → stop → close) — no USB descriptor parsing or raw protocol reverse-engineering needed, unlike ADR-004/ADR-006;
- officially built for macOS, including Apple Silicon (`mac_arm64`) and Intel (`mac_x64`);
- MIT-licensed (confirmed by fetching the actual `license.txt` shipped alongside the binaries — copyright ZWO Company, explicitly permits use/copy/modify/merge/publish/distribute/sublicense/sell, conditioned only on keeping the copyright/permission notice with any copy). This resolves the licensing question that was still open for ADR-004's `libuvc` (that one only needed a system package, not a bundled binary) and is a materially better position than guessing at ZWO's intent from bundling-is-common-practice alone.

ZWO's own download domain (`download.astronomy-imaging-camera.com`) wasn't reachable from the development environment. The SDK (header + macOS binaries + the MIT `license.txt`) was instead fetched from [`indilib/indi-3rdparty/libasi`](https://github.com/indilib/indi-3rdparty/tree/master/libasi), which vendors the exact same ZWO-provided files for INDI's own `indi-asi` driver — same license, same binaries, different distribution channel. `third_party/zwo-sdk/README.md` documents this provenance and how to refresh from ZWO's own site if it becomes reachable later.

**No ZWO camera was available to test against.** Everything here was written against the documented API and verified for what's possible without hardware — see Consequences.

## Decision

Vendor the SDK under `third_party/zwo-sdk/` (header, `mac_arm64`/`mac_x64` dylibs, license) and add `drivers/zwo`, wrapping it behind the existing `ICameraBackend`/`ICameraDevice` contracts, following the same shape as `drivers/euvc`:

- enumerate/open: `ASIGetNumOfConnectedCameras` → `ASIGetCameraProperty` → `ASIOpenCamera` → `ASIInitCamera`. Device id is `"zwo:<CameraID>"` — the SDK's own per-connection handle, stable only for as long as the camera stays connected (fine here, since `open()` always immediately follows an `enumerate()` within the same `rescan()` call, never persisted across app restarts, same scoping every other backend's id already has).
- geometry/format: `ASISetROIFormat` at the sensor's full resolution (rounded to the SDK's `iWidth%8==0`/`iHeight%2==0` requirement), requesting `ASI_IMG_RAW8` or `ASI_IMG_RAW16` (never `ASI_IMG_RGB24`) — genuine raw sensor data, matching this project's Gate C color-correctness goal in a way ADR-006's camera structurally cannot (see that ADR's frame-format note). Bayer pattern comes directly from `ASI_CAMERA_INFO.BayerPattern`, reported by the SDK/camera itself — nothing guessed, unlike the NexImage 5's carried-over `GBRG` assumption.
- controls: `ASIGetNumOfControls`/`ASIGetControlCaps` discovers what this specific camera actually supports at runtime (capability-driven, AGENT.md section 8) and maps `ASI_EXPOSURE`/`ASI_GAIN`/`ASI_GAMMA`/`ASI_WB_R`/`ASI_WB_B`/`ASI_OFFSET` onto the existing `ControlId` set — full parity with the mock's control surface, better than ADR-006 achieved (that one couldn't confirm a working Gamma control from source and dropped it).
- streaming: `ASIStartVideoCapture` once, then a dedicated thread loops blocking `ASIGetVideoData` calls (timeout sized off the current exposure, per the SDK's own documented recommendation) and delivers each successful read as a `FrameBuffer` — no manual USB transfer pool or payload-header parsing needed, since the SDK owns that entirely.

## Consequences

Positive:

- by far the broadest-impact camera backend this project could add — ZWO is the camera most of the target audience actually owns;
- MIT license with a directly-verified text, not an inference — the cleanest license position of any vendored dependency in this project;
- simpler and more maintainable than `drivers/euvc`: no manual descriptor walking, no async USB transfer pool, no payload-header framing — the SDK is the abstraction layer;
- genuine raw sensor data with an SDK-reported (not guessed) Bayer pattern.

Negative — the important one:

- **this backend has never been run against a real ZWO camera.** What's actually been verified without hardware: it compiles against the vendored header; it links against the vendored dylib; the patched dylib's `libusb` dependency resolves correctly at both link time (`otool -L`) and process-launch time (the built app starts cleanly with it loaded); a standalone probe calling `enumerate()` with zero cameras connected returns an empty list without crashing. What has **not** been verified: opening a real camera, `ASISetROIFormat` actually succeeding against real hardware constraints, control get/set against real ranges, and above all whether `ASIGetVideoData` frames actually come back as expected raw Bayer/mono data. Treat this driver as "implemented against documentation, not yet field-tested" until someone runs it against real ZWO hardware — the EUVC bring-up (ADR-006) hit two real bugs (wrong format/frame index, halved buffer size) that only static reading of source code never surfaced and only live testing caught; the same class of surprise should be expected here;
- adds a vendored binary dependency (2 macOS dylibs, ~10MB combined) to the repo, plus a configure-time `install_name_tool`/`codesign` patching step (`drivers/zwo/CMakeLists.txt`) that only runs on macOS and needs Homebrew's `libusb` findable — another moving part relative to `drivers/uvc`'s plain `find_package`;
- macOS-only for now — `third_party/zwo-sdk/` doesn't vendor Linux/Windows binaries, unlike INDI's own `libasi`, which ships those too. Worth adding if/when this project targets those platforms for real.

## Alternatives considered

### Wait for real hardware before writing any code
Rejected: the SDK is thoroughly documented (unlike ADR-006's reverse-engineered protocol, which genuinely required a working reference to trust), the license question is now resolved, and having a structurally-complete, compiling backend ready to validate the moment hardware is available is more useful than nothing. The explicit "unverified" caveat above exists specifically so this isn't mistaken for a finished, trustworthy integration.

### Use INDI's `indi-asi` driver as a subprocess/library instead of talking to ASICamera2 directly
Rejected: unnecessary layer — `indi-asi` itself is just a thin wrapper around the exact same `ASICamera2` calls used here, and pulling in INDI's own framework (`libindi`, its own process/protocol model) would be a far heavier dependency for no functional benefit over calling the SDK directly.
