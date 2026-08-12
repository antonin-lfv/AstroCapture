#pragma once

#include <cstdint>

// Constants for TIS's "EUVC" protocol: real UVC-spec control/descriptor
// semantics carried over a vendor-specific (not UVC-class) USB interface.
// Values read directly from oaCapture's liboacam/euvc sources (GPLv3,
// https://github.com/openastroproject/openastro/tree/master/liboacam/euvc,
// EUVC.h/EUVCusb.h) — see docs/decisions/ADR-006-euvc-proprietary-backend.md
// and docs/development/NEXIMAGE5_HARDWARE_NOTES.md for how/why.
namespace astro::driver::euvc::protocol {

inline constexpr std::uint16_t kTisVendorId = 0x199e;

// bInterfaceClass/bInterfaceSubClass this camera family's control+stream
// interfaces declare instead of the real UVC class (0x0E) — this is
// exactly what keeps macOS's UVCAssistant from claiming it (ADR-006).
inline constexpr std::uint8_t kVendorInterfaceClass = 0xff;
inline constexpr std::uint8_t kVendorInterfaceSubClass = 0x01;

// USB control transfer bmRequestType components (USB 2.0 spec table 9-2).
inline constexpr std::uint8_t kDirOut = 0x00;
inline constexpr std::uint8_t kDirIn = 0x80;
inline constexpr std::uint8_t kTypeVendor = 0x40;
inline constexpr std::uint8_t kTypeClass = 0x20;
inline constexpr std::uint8_t kRecipDevice = 0x00;
inline constexpr std::uint8_t kRecipInterface = 0x01;

// UVC request codes (spec A.8) — identical values whether the interface is
// UVC-class or not, since this is the same protocol underneath.
inline constexpr std::uint8_t kReqSetCur = 0x01;
inline constexpr std::uint8_t kReqGetCur = 0x81;
inline constexpr std::uint8_t kReqGetMin = 0x82;
inline constexpr std::uint8_t kReqGetMax = 0x83;
inline constexpr std::uint8_t kReqGetRes = 0x84;
inline constexpr std::uint8_t kReqGetDef = 0x87;

// Video Control interface class-specific descriptor subtypes (spec A.5).
inline constexpr std::uint8_t kVcHeader = 0x01;
inline constexpr std::uint8_t kVcInputTerminal = 0x02;
inline constexpr std::uint8_t kVcOutputTerminal = 0x03;
inline constexpr std::uint8_t kVcSelectorUnit = 0x04;
inline constexpr std::uint8_t kVcProcessingUnit = 0x05;
inline constexpr std::uint8_t kVcExtensionUnit = 0x06;

// Video Streaming interface class-specific descriptor subtypes (spec A.6).
inline constexpr std::uint8_t kVsInputHeader = 0x01;
inline constexpr std::uint8_t kVsFormatUncompressed = 0x04;
inline constexpr std::uint8_t kVsFrameUncompressed = 0x05;

// Camera Terminal control selectors (spec A.9.4) — only what this backend
// uses; the camera exposes many more TIS-specific ones we don't need yet.
inline constexpr std::uint8_t kCtExposureTimeAbsoluteControl = 0x04;

// Processing Unit control selectors (spec A.9.5).
inline constexpr std::uint8_t kPuBrightnessControl = 0x02;
inline constexpr std::uint8_t kPuGainControl = 0x04;
inline constexpr std::uint8_t kPuGammaControl = 0x09;
inline constexpr std::uint8_t kPuWhiteBalanceComponentControl = 0x0c;
inline constexpr std::uint8_t kPuWhiteBalanceComponentAutoControl = 0x0d;

// Camera Terminal id is fixed at 1 for this camera family (EUVC.h:
// EUVC_CAM_TERMINAL) — unlike the processing unit id, which is read from
// the device's own descriptor since it varies.
inline constexpr std::uint8_t kCameraTerminalId = 1;

// Vendor register bank (distinct from the CT/PU control protocol above) —
// plain single-byte register read/write, used only to ask whether the
// sensor is color or mono before stream format negotiation.
inline constexpr std::uint8_t kRegCameraType = 0x1a;

inline constexpr unsigned int kUsbControlTimeoutMs = 5000;
inline constexpr unsigned int kUsbBulkTimeoutMs = 10000;

}  // namespace astro::driver::euvc::protocol
