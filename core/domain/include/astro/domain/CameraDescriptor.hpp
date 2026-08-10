#pragma once

#include <optional>
#include <string>

namespace astro::domain {

// Never assume model name is unique; `id` is the stable identity.
using CameraId = std::string;

struct CameraDescriptor {
    CameraId id;
    std::string displayName;
    std::string backendId;
    std::optional<std::string> serial;
};

}  // namespace astro::domain
