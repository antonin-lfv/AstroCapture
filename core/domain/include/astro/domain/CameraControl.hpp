#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace astro::domain {

// Canonical semantics for common camera controls. This is intentionally the
// small set the charter names explicitly (AGENT.md section 8); vendor-specific
// controls that don't map to one of these stay behind an escape hatch that
// does not exist yet rather than growing this enum speculatively.
enum class ControlId {
    Exposure,
    Gain,
    WhiteBalance,
    Gamma,
    HardwareROI,
    Temperature,
    Cooler,
    Trigger,
};

enum class ControlValueType {
    Integer,
    Float,
    Enum,
    Bool,
};

using ControlValue = std::variant<std::int64_t, double, bool, std::string>;

// Metadata for one control on one camera. min/max/step/defaultValue apply to
// Integer/Float controls; Enum/Bool controls ignore them.
struct CameraCapability {
    ControlId id;
    ControlValueType valueType;
    bool supported = false;
    bool readOnly = false;
    double minValue = 0.0;
    double maxValue = 0.0;
    double step = 0.0;
    double defaultValue = 0.0;
    bool supportsAuto = false;
    bool mutableWhileStreaming = false;
    std::string unit;
};

}  // namespace astro::domain
