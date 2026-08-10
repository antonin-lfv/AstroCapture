#include "astro/domain/Result.hpp"

namespace astro::domain {

std::string_view to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::DeviceNotFound:
            return "DeviceNotFound";
        case ErrorCode::DeviceBusy:
            return "DeviceBusy";
        case ErrorCode::PermissionDenied:
            return "PermissionDenied";
        case ErrorCode::UnsupportedFormat:
            return "UnsupportedFormat";
        case ErrorCode::UnsupportedControl:
            return "UnsupportedControl";
        case ErrorCode::StreamStartFailed:
            return "StreamStartFailed";
        case ErrorCode::DeviceDisconnected:
            return "DeviceDisconnected";
        case ErrorCode::InvalidFrame:
            return "InvalidFrame";
        case ErrorCode::RecordingOpenFailed:
            return "RecordingOpenFailed";
        case ErrorCode::DiskWriteFailed:
            return "DiskWriteFailed";
        case ErrorCode::DiskTooSlow:
            return "DiskTooSlow";
        case ErrorCode::ModelLoadFailed:
            return "ModelLoadFailed";
        case ErrorCode::InternalInvariantViolation:
            return "InternalInvariantViolation";
    }
    return "Unknown";
}

}  // namespace astro::domain
