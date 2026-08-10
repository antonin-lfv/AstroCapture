#pragma once

#include <vector>

#include "astro/domain/CameraControl.hpp"
#include "astro/domain/CameraDescriptor.hpp"
#include "astro/domain/Frame.hpp"
#include "astro/domain/Result.hpp"
#include "astro/domain/StreamMode.hpp"

namespace astro::domain {

// Receives frames from a streaming device. Implementations must return
// quickly: this is called from the acquisition path.
class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual void onFrame(const FrameBuffer& frame) = 0;
};

// One open camera. Capability-driven: callers must consult capabilities()
// before assuming a control exists rather than calling setControl blind.
class ICameraDevice {
public:
    virtual ~ICameraDevice() = default;

    virtual CameraDescriptor descriptor() const = 0;
    virtual std::vector<StreamMode> streamModes() const = 0;
    virtual std::vector<CameraCapability> capabilities() const = 0;

    virtual Result<void> configure(const StreamConfiguration& configuration) = 0;
    virtual Result<void> start(FrameSink& sink) = 0;
    virtual Result<void> stop() = 0;

    virtual Result<ControlValue> getControl(ControlId id) const = 0;
    virtual Result<void> setControl(ControlId id, const ControlValue& value) = 0;
};

}  // namespace astro::domain
