# ADR-006 — Proprietary-protocol (`euvc`) backend, superseding UVC mode for the NexImage 5 on macOS

## Status
Accepted.

## Context

ADR-004 chose `libuvc` for the first real-camera backend; ADR-005 fixed the app bundling/TCC issue that kept it from even trying. Live-testing after both landed still failed: `uvc_open()` returned `LIBUSB_ERROR_ACCESS` from inside a properly authorized, ad-hoc-signed `.app` — identical to an unsigned CLI probe. `ioreg` showed why: macOS's own `UVCAssistant` system extension (`CoreMediaIO.framework`, running as `_cmiodalassistants`) claims **exclusive** ownership of any USB interface that declares UVC class the moment it appears. That's an OS-level exclusivity rule, not a permission gate — no signing, entitlement, or TCC state changes it. Full write-up: `docs/development/NEXIMAGE5_HARDWARE_NOTES.md`.

Consequence: the NexImage 5, switched into UVC mode specifically so `libuvc` could see it, becomes *less* usable on macOS than it was in its factory state, where it isn't UVC-class at all and nothing claims it.

Investigating oaCapture (already proven to stream this exact camera on this machine) confirmed it never uses UVC mode for this camera. Its `liboacam/euvc` driver talks to the **factory/proprietary mode** directly: a vendor-specific USB interface (`bInterfaceClass=0xFF`, `bInterfaceSubClass=1`) that macOS never claims, carrying UVC-spec-shaped class-specific descriptors and control semantics (Camera Terminal / Processing Unit selectors, `GET_CUR`/`SET_CUR`/etc. request codes identical to the real UVC spec) without ever declaring the OS-recognized UVC interface class. `EUVCconnect.c` in that source has a comment naming this exact PID (`0x8207`) as "the Neximage 5."

## Decision

Revert the camera to proprietary mode (done) and build a new backend, `drivers/euvc`, that talks this protocol directly over raw `libusb` control and bulk transfers — no `libuvc`, no OS UVC recognition, so nothing macOS owns competes with it:

- interface/descriptor discovery: find the `0xFF/1` interface, walk its class-specific descriptor bytes (same `bDescriptorSubtype` layout as real UVC's VC_HEADER/VC_INPUT_TERMINAL/VC_PROCESSING_UNIT/VS_INPUT_HEADER/VS_FORMAT_UNCOMPRESSED/VS_FRAME_UNCOMPRESSED) to discover terminal ID, processing unit ID, bulk streaming endpoint, and frame geometry — all read from the device, none hardcoded per camera model;
- controls: raw `libusb_control_transfer` with `USB_CTRL_TYPE_CLASS|USB_RECIP_INTERFACE`, `wValue = selector<<8`, `wIndex = terminalOrUnitId<<8` — the same UVC Camera Terminal (`EXPOSURE_TIME_ABSOLUTE`) / Processing Unit (`GAIN`, `BRIGHTNESS`, `GAMMA`, `WHITE_BALANCE_COMPONENT`) selectors and `GET_CUR`/`GET_MIN`/`GET_MAX`/`GET_RES`/`SET_CUR` request codes already implemented for `drivers/uvc` — only the transport changes, not the control semantics;
- streaming: a pool of asynchronous `libusb` bulk transfers against the discovered IN endpoint, serviced by a dedicated event-handling thread; each payload starts with a 2-byte header (length, flags — bit 0 frame-ID toggle, bit 1 end-of-frame, bit 6 error) whose end-of-frame bit marks a complete frame ready to hand to `FrameSink`.

The stream format negotiates under the "YUY2"/"Y800" FourCC (the descriptor's own labels), which is what led ADR-006's first draft to assume no raw Bayer was available here — matching what UVC mode's `drivers/uvc` would also have gotten from a real webcam. Live testing corrected that: every complete (end-of-frame-flagged) payload from this camera is exactly `width * height` bytes — one byte per pixel, not YUY2's two — meaning the "YUY2" tag is a compatibility label, not an accurate description of the wire data. The `euvc` device delivers these bytes as `PixelStorage::Bayer8` (color units) or `PixelStorage::Mono8` (mono units) unmodified, reusing the existing Bayer8 debayer path (`astro::processing::debayerBilinear`) rather than converting anything. Bayer pattern is set to `BayerPattern::GBRG` — a best guess carried over from oaCapture's own historical bug reports about this camera family ("GBRG being the correct pattern for the TIS cameras that have been tested"), pending direct verification against a real color target on this unit.

`drivers/uvc` (ADR-004) is not removed: it stays correct and useful for actual UVC-class devices and other platforms — Linux's `uvcvideo` kernel driver does not exhibit the same exclusive-claim behavior for third-party access (per the INDI/Stargazers Lounge forum research already in the hardware notes doc), so a camera that only ever operates in UVC mode there is unaffected by this ADR. `PreviewController::rescan()` tries both backends' `enumerate()` and merges the results, the same way it already merges any pool of real candidates.

## Consequences

Positive:

- unblocks real camera use on macOS at all for this camera, which ADR-004 and ADR-005 together could not;
- reuses ADR-004's control-selector logic almost unchanged — only the transport (raw `libusb` vs `libuvc`) and interface discovery (manual descriptor walk vs OS/`libuvc`-parsed) differ;
- validated against a real, working reference implementation (oaCapture) rather than protocol guesswork;
- delivers real raw Bayer8 data (see the frame-format note above) — better than ADR-006's original draft expected, and better than UVC mode/`drivers/uvc` could ever get from this camera even if `UVCAssistant` weren't in the way, since a generic UVC client has no reason to suspect a "YUY2"-labeled stream isn't really YUY2.

Negative:

- another camera-specific protocol to maintain, on top of `libuvc`'s generic one — this one only helps cameras that speak this exact TIS/Imaging-Source dialect, not UVC-class devices generally;
- reimplements a slice of UVC descriptor parsing that `libuvc` already does for real UVC devices, because it has to be done manually against a non-UVC-class interface;
- the Bayer pattern (`GBRG`) is a carried-over best guess, not yet directly verified against this unit with a real color target.

## Alternatives considered

### Keep pushing on UVC mode (ADR-004/005) via a different macOS API
Rejected: `UVCAssistant`'s exclusive claim is an OS design choice covering CoreMediaIO's whole UVC pipeline, not a bug or a permission check — there is no documented Apple API to release or bypass it for a third-party app.

### Reverse-engineer the protocol from scratch via USB traffic capture
Rejected: unnecessary. oaCapture's GPLv3 `liboacam/euvc` source already documents the exact protocol, tested against this camera specifically — reimplementing independently would only add risk (misreading captured traffic) for no benefit over reading working, proven source.
