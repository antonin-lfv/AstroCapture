#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include "UvcCameraDevice.hpp"

#include <chrono>
#include <vector>

using astro::driver::uvc::UvcCameraDevice;

// Forwards AVFoundation's sample buffer callback into the owning
// UvcCameraDevice. `device` is `assign` (unretained): UvcCameraDevice::stop()
// clears it before the device can be destroyed, so a queued callback racing
// with teardown finds it null instead of dangling.
@interface UvcSampleBufferRelay : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, assign) UvcCameraDevice* device;
@end

@implementation UvcSampleBufferRelay

- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
            fromConnection:(AVCaptureConnection*)connection {
    (void)output;
    (void)connection;
    if (self.device == nullptr) {
        return;
    }

    CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (pixelBuffer == nullptr) {
        return;
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    const auto width = static_cast<std::uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
    const auto height = static_cast<std::uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
    const auto strideBytes = static_cast<std::uint32_t>(CVPixelBufferGetBytesPerRow(pixelBuffer));
    const auto* base = static_cast<const std::byte*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    const std::size_t size = static_cast<std::size_t>(strideBytes) * height;

    if (base != nullptr) {
        self.device->deliverFrame(width, height, strideBytes, base, size);
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}

@end

namespace astro::driver::uvc {

using namespace astro::domain;

namespace {

// 640x480@30 is the one mode this backend advertises today. AVFoundation
// devices generally support far more; enumerating AVCaptureDeviceFormat and
// mapping each to a StreamMode is future work once something needs it.
StreamMode makeDefaultMode() {
    StreamMode mode;
    mode.width = 640;
    mode.height = 480;
    mode.storage = PixelStorage::BGRA32;
    mode.bayer = BayerPattern::None;
    mode.validBits = 8;
    mode.minFps = 1.0;
    mode.maxFps = 30.0;
    return mode;
}

}  // namespace

UvcCameraDevice::UvcCameraDevice(CameraDescriptor descriptor, std::string avUniqueId)
    : descriptor_(std::move(descriptor)), avUniqueId_(std::move(avUniqueId)) {}

UvcCameraDevice::~UvcCameraDevice() { stop(); }

CameraDescriptor UvcCameraDevice::descriptor() const { return descriptor_; }

std::vector<StreamMode> UvcCameraDevice::streamModes() const { return {makeDefaultMode()}; }

std::vector<CameraCapability> UvcCameraDevice::capabilities() const {
    // AVFoundation exposes exposure/focus/white-balance through a separate,
    // lower-level API than plain video capture; not wired up yet.
    return {};
}

Result<void> UvcCameraDevice::configure(const StreamConfiguration& /*configuration*/) {
    if (running_.load()) {
        return Result<void>::failure(
            {ErrorCode::InternalInvariantViolation, "UvcCameraDevice cannot be reconfigured while streaming", ""});
    }
    // Single advertised mode: nothing to validate against yet.
    configured_ = true;
    return Result<void>::success();
}

Result<void> UvcCameraDevice::start(FrameSink& sink) {
    if (running_.load()) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "UvcCameraDevice is already streaming", ""});
    }
    if (!configured_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "configure() must succeed before start()", ""});
    }

    NSString* uniqueId = [NSString stringWithUTF8String:avUniqueId_.c_str()];
    AVCaptureDevice* device = [AVCaptureDevice deviceWithUniqueID:uniqueId];
    if (device == nil) {
        return Result<void>::failure({ErrorCode::DeviceNotFound, "AVCaptureDevice no longer available", ""});
    }

    NSError* error = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (input == nil) {
        const std::string message = error != nil ? [[error localizedDescription] UTF8String] : "unknown error";
        return Result<void>::failure({ErrorCode::StreamStartFailed, "Failed to open AVCaptureDeviceInput", message});
    }

    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    if (![session canAddInput:input]) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "AVCaptureSession rejected the device input", ""});
    }
    [session addInput:input];

    AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
    // 32BGRA is the pixel format AVFoundation reliably honors across
    // cameras; its in-memory byte order (B,G,R,A) matches Qt's
    // QImage::Format_ARGB32 on little-endian, so the app layer can wrap the
    // bytes directly with no conversion (see FramePreviewImage.cpp).
    output.videoSettings = @{(NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
    output.alwaysDiscardsLateVideoFrames = YES;

    UvcSampleBufferRelay* relay = [[UvcSampleBufferRelay alloc] init];
    relay.device = this;
    dispatch_queue_t queue = dispatch_queue_create("astro.driver.uvc.samplebuffer", DISPATCH_QUEUE_SERIAL);
    [output setSampleBufferDelegate:relay queue:queue];

    if (![session canAddOutput:output]) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "AVCaptureSession rejected the video output", ""});
    }
    [session addOutput:output];

    {
        std::lock_guard lock(sinkMutex_);
        sink_ = &sink;
    }
    sequence_.store(0, std::memory_order_relaxed);
    session_ = session;
    relay_ = relay;
    running_.store(true, std::memory_order_relaxed);
    [session startRunning];

    return Result<void>::success();
}

Result<void> UvcCameraDevice::stop() {
    if (!running_.exchange(false, std::memory_order_relaxed)) {
        return Result<void>::success();
    }

    {
        std::lock_guard lock(sinkMutex_);
        sink_ = nullptr;
    }

    if (relay_ != nullptr) {
        relay_.device = nullptr;
    }
    if (session_ != nullptr) {
        [session_ stopRunning];
    }
    session_ = nullptr;
    relay_ = nullptr;

    return Result<void>::success();
}

Result<ControlValue> UvcCameraDevice::getControl(ControlId /*id*/) const {
    return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "UvcCameraDevice exposes no controls yet", ""});
}

Result<void> UvcCameraDevice::setControl(ControlId /*id*/, const ControlValue& /*value*/) {
    return Result<void>::failure({ErrorCode::UnsupportedControl, "UvcCameraDevice exposes no controls yet", ""});
}

void UvcCameraDevice::deliverFrame(std::uint32_t width, std::uint32_t height, std::uint32_t strideBytes,
                                    const std::byte* data, std::size_t size) {
    std::lock_guard lock(sinkMutex_);
    if (sink_ == nullptr) {
        return;
    }

    // CVPixelBuffer memory belongs to AVFoundation's pool and is reused
    // once this callback returns, so it must be copied before leaving.
    std::vector<std::byte> bytes(data, data + size);

    FrameBuffer frame;
    frame.descriptor.width = width;
    frame.descriptor.height = height;
    frame.descriptor.strideBytes = strideBytes;
    frame.descriptor.storage = PixelStorage::BGRA32;
    frame.descriptor.bayer = BayerPattern::None;
    frame.descriptor.validBits = 8;
    frame.metadata.sequence = sequence_.fetch_add(1, std::memory_order_relaxed);
    frame.metadata.hostArrival = std::chrono::steady_clock::now();
    frame.storage = std::make_shared<const BufferStorage>(std::move(bytes));

    sink_->onFrame(frame);
}

}  // namespace astro::driver::uvc
