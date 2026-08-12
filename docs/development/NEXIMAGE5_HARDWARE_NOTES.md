# NexImage 5 / TIS USB2 camera hardware notes

Findings from bringing up a real Celestron NexImage 5 against Gate C (see
`docs/product/ROADMAP.md`, `docs/decisions/ADR-004-uvc-camera-backend.md`,
`docs/decisions/ADR-005-macos-camera-permission.md`,
`docs/decisions/ADR-006-euvc-proprietary-backend.md`). Kept separate from
the ADRs because this is reference material discovered along the way, not
a decision record itself — update it as `drivers/euvc` matures further.

## Identity

The Celestron NexImage 5 is not Celestron's own hardware: it's a rebadged
**The Imaging Source (TIS) DFK72** — USB `idVendor=0x199E`, `idProduct`
`0x8207` in its factory/default mode, `0x8307` when switched to UVC mode
(see below). Confirmed on-device (`ioreg`, `system_profiler`) and against
oaCapture's own source (`liboacam/euvc/EUVCoacam.c`), which contains a
`{ 0x8207, "DFK72", EUVC_CAM_DFK72 }` table entry.

## Two protocol modes, one physical camera

The camera's EEPROM stores a mode flag toggling between two completely
different USB personalities:

- **Proprietary / "EUVC" mode** (factory default, `idProduct=0x8207`): the
  camera exposes a **vendor-specific** USB interface
  (`bInterfaceClass=0xFF`, `bInterfaceSubClass=1`) carrying UVC-spec-shaped
  class-specific descriptors (Video Control header, input terminal,
  processing unit, video streaming interface — same `bDescriptorSubtype`
  layout as real UVC) but declared under a non-UVC interface class. No OS
  camera stack recognizes or claims it — see "macOS and UVCAssistant"
  below for why that specific property matters.
- **UVC mode** (`idProduct=0x8307`): the exact same descriptors, but the
  interface is declared as real UVC class (`0x0E`), so generic UVC tools
  (`libuvc`, this project's `drivers/uvc`) and OS camera frameworks
  recognize it immediately.

Switching between them is a **2-value EEPROM write**, not a firmware
flash: the product ID's high byte (`0x82` proprietary / `0x83` UVC) and a
1-byte mode flag at `EEPROM_SIZE(16384) - DATA_SIZE(512) - 1 = 0x3DFF`.
Protocol read directly from TIS's own Apache-2.0 tool source
(`TheImagingSource/tcam-firmware-update`, `src/Usb2Camera.cpp`,
`Usb2Camera::set_mode()`/`get_mode()`) — that tool itself is Linux-only
(needs `libudev`), but the underlying `libusb` control-transfer sequence
it sends is portable and was reimplemented standalone for this session's
testing. Confirmed reversible in both directions on this exact unit.

## macOS and UVCAssistant — why `drivers/uvc` (ADR-004) can't reach this camera in UVC mode

Switching the camera into UVC mode does **not** make it usable via
`libuvc`/raw `libusb` on macOS. The moment the interface declares UVC
class, macOS's own system extension —
`/System/Library/Frameworks/CoreMediaIO.framework/Versions/A/Resources/UVCAssistant.systemextension`,
running as the `_cmiodalassistants` user — claims the interface
**exclusively** (`ioreg` shows `"UsbExclusiveOwner" = "pid <n>, UVCAssistant"`).
No app-level fix (code signing, TCC/Camera permission, entitlements) gets
that claim released; it's an OS design choice, not a permission gate.
Confirmed by: `uvc_open()` failing with `LIBUSB_ERROR_ACCESS` identically
whether tried from an unsigned CLI tool or from a properly bundled,
ad-hoc-signed, TCC-authorized `.app` (see ADR-005 — ADR-005's own
permission work was necessary but not sufficient; it fixed a real,
separate problem — TCC never registering the app at all — but didn't fix
this one, which was only discovered afterward).

Net effect: on macOS specifically, **only the proprietary/vendor-class
mode is usable** by a third-party app. UVC mode is only useful here for
other platforms/tools where nothing claims it exclusively (Linux: works
through the standard `uvcvideo` kernel driver via V4L2, per the INDI/
Stargazers Lounge forum threads found during Gate C research).

## What oaCapture actually does (and why it already worked)

oaCapture never switches this camera into UVC mode. Its
`liboacam/euvc/` driver ("EUVC" = the vendor-class protocol described
above, not a typo for UVC) talks to it in **proprietary mode**, over raw
`libusb` control and bulk transfers, using standard UVC Camera
Terminal / Processing Unit control selectors
(`EUVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL`, `EUVC_PU_GAIN_CONTROL`, etc. —
same semantics as the UVC spec, just sent manually instead of through an
OS-parsed UVC descriptor) to a `bInterfaceClass=0xFF` interface macOS
never touches. Source:
https://github.com/openastroproject/openastro/tree/master/liboacam/euvc
— `EUVCconnect.c` (interface/descriptor discovery, `libusb_claim_interface`),
`EUVCcontroller.c` (control set, e.g. exposure/gain via
`libusb_control_transfer` with `USB_CTRL_TYPE_CLASS|USB_RECIP_INTERFACE`),
`EUVCcallback.c` (frame delivery). `EUVCconnect.c` even has a
camera-specific comment: `// Set overflowTransmit just for the Neximage 5.`
gated on `productId == 0x8207` — direct confirmation this driver was
written/tested against this exact camera, not just "the same chip family."

This project's `drivers/euvc` backend (successor to the `drivers/uvc`
libuvc attempt, for this camera specifically) follows the same approach:
proprietary-mode, vendor-class interface, raw
`libusb_control_transfer`/bulk reads, UVC-spec control selectors —
implemented and live-tested against this exact camera; see the frame
format section below for what it actually delivers.

## Frame format — labeled "YUY2", actually raw Bayer8

`EUVCconnect.c`'s `_scanEUVCStream()` matches the format descriptor's
FourCC against **YUY2/UYVY** (color cameras) or **Y800** (mono cameras)
— never a raw Bayer FourCC — which is what first suggested this camera
only ever outputs already-demosaiced YUV, the same ceiling a generic UVC
client (`drivers/uvc`, any off-the-shelf UVC viewer) would hit.

Live testing of `drivers/euvc` against this exact unit found otherwise:
every complete, end-of-frame-flagged bulk payload is exactly
`width * height` bytes (2592×1944 = 5,038,848) — **one byte per pixel**,
not YUY2's two. That's raw Bayer8, not YUV. The "YUY2" FourCC in this
camera's descriptor is a compatibility label, not an accurate format
description — this firmware appears to advertise a generic-sounding tag
so naive UVC tooling doesn't choke on an unrecognized format, while
actually shipping raw sensor data. This also finally explains oaCapture's
own long-standing bug reports about this camera family being about
**Bayer pattern** mismatches (GRBG vs GBRG) rather than YUV chroma
issues — they were debating how to interpret raw mosaic data all along,
not how to decode YUV.

Practical effect: raw, pre-demosaic sensor data **is** available from
this camera via `drivers/euvc` (proprietary mode only — see above; UVC
mode's `libuvc`/generic tooling would still only ever see it as YUY2 and
never get the correction applied here). `drivers/euvc` delivers frames as
`PixelStorage::Bayer8` with `BayerPattern::GBRG` — GBRG is carried over
from oaCapture's own historical findings for this camera family, not yet
independently re-verified against a real color target on this specific
unit; if live images look color-shifted, try the other three 2x2 Bayer
patterns before suspecting anything else in the pipeline.

Two descriptor quirks worth remembering if this format-matching code is
ever touched again:
- `dwMaxVideoFrameBufferSize` (the descriptor's own declared max frame
  size) reads as `width * height`, i.e. exactly half the byte count
  you'd expect for a real 2-byte/pixel YUY2 frame of that resolution —
  consistent with it actually describing the true (1 byte/pixel) Bayer8
  payload despite the format being tagged YUY2. `EuvcCameraDevice` does
  not trust this field for allocating transfer buffers because of this;
  it computes buffer size directly from geometry instead.
- The VS_COMMIT_CONTROL negotiation must carry the real `bFormatIndex`/
  `bFrameIndex` read from the device's own VS_FORMAT_UNCOMPRESSED/
  VS_FRAME_UNCOMPRESSED descriptors — sending `0`/`0` (as if the camera
  only had one anonymous format) compiles and transfers cleanly with no
  USB-level error, but the camera silently never starts sending bulk
  data at all. This was the first bring-up bug found; the second (the
  buffer-size quirk above) only became visible after fixing this one.

## Useful diagnostic commands (macOS)

```sh
# Raw USB enumeration (works regardless of protocol mode)
ioreg -p IOUSB -l -w0 | grep -i "neximage\|idVendor\|idProduct"

# Whether AVFoundation/CoreMediaIO currently exposes it as a camera
system_profiler SPCameraDataType

# Whether something (e.g. UVCAssistant) has claimed exclusive USB access
ioreg -l | grep -i "UsbExclusiveOwner"

# Whether the app-level TCC Camera decision was actually recorded
sqlite3 "$HOME/Library/Application Support/com.apple.TCC/TCC.db" \
  "SELECT service, client, auth_value FROM access WHERE service='kTCCServiceCamera';"
```
