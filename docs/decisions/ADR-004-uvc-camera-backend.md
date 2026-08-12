# ADR-004 — libuvc-based backend for the first real camera (Gate C)

## Status
Proposed.

## Context

Gate C requires a real, non-mock camera backend, validated against a Celestron NexImage 5. Investigation on the reference machine established:

- the NexImage 5 is Celestron-branded The Imaging Source hardware (USB `idVendor=0x199E`, `idProduct=0x8207`);
- it enumerates as a raw USB device on macOS but is invisible to `AVFoundation` (`system_profiler SPCameraDataType` does not list it) — it is not currently presenting a UVC video interface to the OS's camera stack;
- no official Celestron or The Imaging Source SDK exists for macOS; the vendor's own tooling (`tiscamera`, `tcam-firmware-update`) targets Linux only (the latter also depends on `libudev`);
- despite that, **oaCapture — an existing open-source, cross-platform planetary-capture application — already streams this exact camera on this machine today**, with a known defect: frames come through without color. oaCapture's own history attributes this to a wrong Bayer pattern assumption (GBRG) for TIS color sensors, not a hardware limitation — the NexImage 5's sensor (Micron/Aptina MT9P031) is a color sensor.
- oaCapture reaches TIS cameras through `libuvc`, a small BSD-3-Clause, cross-platform (Linux/macOS/Windows) library that speaks the USB Video Class protocol directly over `libusb`, rather than through any OS-native high-level camera framework.
- a generic `AVFoundation`-based UVC/webcam backend previously existed in this repo (`drivers/uvc`, commit `bb4c4b0`) and was deliberately removed (commit `e998251`): it exposed no exposure/gain control, and macOS's built-in webcam kept winning the single-real-camera auto-connect over the mock camera, breaking the mock-driven dev workflow.

## Decision

Build the first real-hardware camera backend (`drivers/uvc`, replacing the removed AVFoundation one) on top of `libuvc` instead of a per-OS native capture framework.

Initial scope:

- enumerate/open/stream via `libuvc`, gated the same way the removed backend was (`ASTRO_ENABLE_DRIVER_UVC` or equivalent), implementing the existing `ICameraBackend`/`ICameraDevice` contracts;
- expose exposure/gain through `libuvc`'s UVC Camera Terminal / Processing Unit control API where the device advertises support for them — this is the capability the removed backend lacked, and what real manual control over an astro camera requires;
- deliver frames in whatever raw/Bayer format the device advertises; correct TIS Bayer-pattern decoding (the GBRG issue seen in oaCapture) is a fast-follow, not a blocker for first light — Gate C's color-correctness exit criterion is not closed until that follow-up lands;
- real-candidate discovery must not regress the bug the old backend caused: it must not let the host's own built-in/other generic webcams win the single-real-camera auto-connect ahead of (or instead of) the mock camera during dev builds. This needs an explicit filter (e.g. require UVC exposure/gain control support, or a vendor allow-list) rather than "any UVC device wins," which is what broke last time.

The one-time "switch this specific unit into UVC mode via `tcam-firmware-update`" path researched alongside this decision is **not required**: oaCapture already streams this unit without it, so it is dropped from scope and only noted here in case a future unit needs it.

## Consequences

Positive:

- one implementation covers macOS/Windows/Linux, instead of a macOS-only `AVFoundation` backend needing separate Windows/Linux equivalents later;
- direct access to standards-based UVC controls (exposure/gain) that high-level OS camera frameworks (`AVFoundation`) don't surface;
- a working reference client (oaCapture) exists to compare behavior against when something doesn't match spec;
- permissively licensed (`libuvc` BSD-3-Clause, `libusb` LGPL-2.1 — dynamically linked, no copyleft obligation on AstroCapture itself, but packaging must keep it dynamically linked and ship license notices).

Negative:

- new third-party native dependency (`libuvc` + `libusb`) to vendor/package per OS, on top of Qt;
- TIS Bayer-pattern color decoding is unresolved and must be solved separately before Gate C's color-correctness criterion is met;
- device-filtering logic to keep mock-camera dev workflow intact adds real complexity, not just "enumerate and connect";
- only covers cameras that speak UVC; a future reference camera using a fully proprietary (non-UVC) protocol would need a different backend regardless.

## Alternatives considered

### Revive the removed `AVFoundation` backend as-is
Rejected: already proven insufficient (commit `e998251`) — no exposure/gain, and macOS-only, so Windows/Linux would need a second implementation later anyway.

### Vendor SDK (Celestron / The Imaging Source)
Rejected: no macOS SDK exists for this camera from either vendor; TIS's own open-source stack (`tiscamera`) is Linux/V4L2-specific and not portable to macOS without substantial rework.

### `tcam-firmware-update`-driven UVC mode switch + native per-OS capture
Not pursued for now: unnecessary, since oaCapture already streams this exact unit without any firmware modification. Documented here as a fallback if a future unit turns out to need it.
