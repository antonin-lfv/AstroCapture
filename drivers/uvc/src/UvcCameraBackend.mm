#import <AVFoundation/AVFoundation.h>

#include "astro/driver/uvc/UvcCameraBackend.hpp"

#include "UvcCameraDevice.hpp"

namespace astro::driver::uvc {

using namespace astro::domain;

namespace {

constexpr const char* kBackendId = "uvc";

NSArray<AVCaptureDeviceType>* discoverableDeviceTypes() {
    return @[ AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternal ];
}

CameraDescriptor makeDescriptor(AVCaptureDevice* device) {
    CameraDescriptor descriptor;
    descriptor.id = [[device uniqueID] UTF8String];
    descriptor.displayName = [[device localizedName] UTF8String];
    descriptor.backendId = kBackendId;
    return descriptor;
}

}  // namespace

std::string UvcCameraBackend::id() const { return kBackendId; }

std::vector<CameraDescriptor> UvcCameraBackend::enumerate() {
    AVCaptureDeviceDiscoverySession* discovery =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:discoverableDeviceTypes()
                                                                 mediaType:AVMediaTypeVideo
                                                                  position:AVCaptureDevicePositionUnspecified];

    std::vector<CameraDescriptor> descriptors;
    for (AVCaptureDevice* device in discovery.devices) {
        descriptors.push_back(makeDescriptor(device));
    }
    return descriptors;
}

std::unique_ptr<ICameraDevice> UvcCameraBackend::open(const CameraId& id) {
    NSString* uniqueId = [NSString stringWithUTF8String:id.c_str()];
    AVCaptureDevice* device = [AVCaptureDevice deviceWithUniqueID:uniqueId];
    if (device == nil) {
        return nullptr;
    }
    return std::make_unique<UvcCameraDevice>(makeDescriptor(device), id);
}

}  // namespace astro::driver::uvc
