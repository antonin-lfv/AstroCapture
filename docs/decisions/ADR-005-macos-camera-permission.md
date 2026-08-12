# ADR-005 — macOS app bundle + AVFoundation permission handshake for the UVC backend

## Status
Accepted.

## Context

The libuvc-based real-camera backend (ADR-004) compiled and linked cleanly, but live-tested at zero devices found. Investigation on the reference machine established, in order:

- the NexImage 5 was still in its factory "proprietary" protocol mode, not UVC — switching it to UVC mode (a documented, reversible EEPROM flag write, see the mode-switch work in this session) made it enumerate as a standard composite UVC device (`bDeviceClass=0xEF` with an Interface Association Descriptor);
- with the device now genuinely UVC, `libuvc`'s `uvc_open()` still failed with `LIBUSB_ERROR_ACCESS`, even though plain `libusb_open()` on the same device succeeded before the mode switch;
- `ioreg` showed no IOKit driver (Apple's own or otherwise) had claimed any interface on the device — ruling out a kernel-driver conflict;
- `System Settings > Privacy & Security > Camera` had no entry for AstroCapture at all, and no way to add one manually;
- the built `AstroCapture` binary was a bare Mach-O executable, not a `.app` bundle — no `Info.plist`, no `NSCameraUsageDescription`, no stable code identity.

Conclusion: macOS's TCC privacy subsystem gates raw USB access to Video-class (UVC) interfaces the same way it gates AVFoundation capture, but it only ever registers an app for that gate (and only ever shows the permission prompt) when the app goes through AVFoundation's own authorization API at least once. A bare executable never does that, so TCC has no record of it, access is silently refused, and there is nothing to toggle in Settings because the app was never asked about in the first place.

## Decision

1. Build `AstroCapture` as a real `.app` bundle (`MACOSX_BUNDLE` on the Qt executable target) with a custom `Info.plist` declaring `NSCameraUsageDescription` — mandatory before AVFoundation's authorization API will do anything but auto-deny.
2. Add a small Objective-C++ shim (`CameraPermission.mm`) that checks `AVCaptureDevice.authorizationStatusForMediaType:` and, if `NotDetermined`, fires `requestAccessForMediaType:completionHandler:` — asynchronously, never blocking the calling thread (blocking would freeze the exact run loop the system prompt needs to render on, deadlocking instead of showing the dialog).
3. `PreviewController::rescan()` checks authorization status before attempting `UvcCameraBackend::enumerate()`: only proceeds when `Authorized`, fires the request once when `NotDetermined`, and otherwise skips straight to the mock-camera fallback exactly as it does today when no real backend is available — first-run UX is "click Rescan, approve the system prompt, click Rescan again."
4. Ad-hoc codesign (`codesign --sign -`) the bundle as a post-build step, so the app keeps a stable local identity across rebuilds instead of macOS treating each fresh unsigned binary as a new app and losing (or re-prompting for) its Camera decision.
5. A companion `CameraPermission.cpp` no-op implementation (`Authorized` unconditionally, `request` a no-op) is compiled on non-Apple platforms — this entire mechanism is macOS-specific; see the "Alternatives" note on Linux/Windows below.

## Consequences

Positive:

- the UVC backend can actually reach hardware on macOS at all, which nothing in ADR-004 could fix on its own;
- the permission flow matches what every other AVFoundation-based macOS app does, so it's a one-time, expected system prompt rather than a silent, undiagnosable failure;
- ad-hoc signing needs no Apple Developer account, so this costs nothing during development.

Negative:

- first real-camera use now needs two clicks of Rescan (request, then retry) instead of one — acceptable for a permission grant that only happens once per machine;
- ad-hoc signing is not a substitute for real signing/notarization before this is ever distributed to someone else — packaging for distribution needs its own follow-up;
- one more platform-conditional source file pair (`CameraPermission.mm` / `.cpp`) to keep in sync if the authorization contract ever changes.

## Alternatives considered

### Do nothing, tell users to grant Camera access manually
Rejected: not possible as observed — TCC never lists an app that hasn't gone through the AVFoundation authorization API, so there is nothing to grant manually until this fix lands.

### Skip AVFoundation, request the permission some other way
Rejected: `AVCaptureDevice`'s authorization API is the only public, documented path that registers an app with the Camera privacy gate on macOS; there is no separate raw-USB-specific permission API.

### Cross-platform note (not a decision, context for future work)
This entire mechanism is macOS-only. Linux has no TCC-equivalent gate on USB device access — it's plain Unix file permissions on `/dev/bus/usb/...`, conventionally opened up via a udev rule shipped with the package (no code, no prompt). Windows has no camera-privacy gate blocking classic Win32/libusb access either, but needs a WinUSB driver bound to the interface instead of the OS's own UVC class driver — typically a one-time Zadig-driven driver swap, or targeting Media Foundation/DirectShow instead of libusb directly. Neither needs anything like this ADR's fix; they're tracked as separate future work, not implemented here.
