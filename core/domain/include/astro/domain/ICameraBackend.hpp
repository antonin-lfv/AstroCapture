#pragma once

#include <memory>
#include <string>
#include <vector>

#include "astro/domain/CameraDescriptor.hpp"
#include "astro/domain/ICameraDevice.hpp"

namespace astro::domain {

// A technology/provider capable of enumerating devices (e.g. mock, or a
// real camera's vendor SDK). Drivers depend on this contract; the reverse
// never happens.
class ICameraBackend {
public:
    virtual ~ICameraBackend() = default;

    virtual std::string id() const = 0;
    virtual std::vector<CameraDescriptor> enumerate() = 0;
    // Returns nullptr if `id` is not currently enumerable by this backend.
    virtual std::unique_ptr<ICameraDevice> open(const CameraId& id) = 0;
};

}  // namespace astro::domain
