#include "astro/driver/uvc/UvcCameraBackend.hpp"

#include <iomanip>
#include <sstream>

#include <libuvc/libuvc.h>

#include "UvcCameraDevice.hpp"
#include "UvcFormat.hpp"

namespace astro::driver::uvc {

using namespace astro::domain;

namespace {

constexpr const char* kBackendId = "uvc";

// "uvc:<vid hex>:<pid hex>[:<serial>]" — self-contained enough to
// re-uvc_find_device() the same physical unit from open(), without needing
// a persistent enumeration index.
std::string makeId(std::uint16_t vid, std::uint16_t pid, const char* serial) {
    std::ostringstream out;
    out << "uvc:" << std::hex << std::setfill('0') << std::setw(4) << vid << ':' << std::setw(4) << pid;
    if (serial != nullptr && serial[0] != '\0') {
        out << ':' << serial;
    }
    return out.str();
}

bool parseId(const CameraId& id, int& vid, int& pid, std::string& serial) {
    constexpr std::string_view kPrefix = "uvc:";
    if (id.rfind(kPrefix, 0) != 0) {
        return false;
    }
    const auto firstColon = id.find(':', kPrefix.size());
    if (firstColon == std::string::npos) {
        return false;
    }
    const auto secondColon = id.find(':', firstColon + 1);
    const std::string vidHex = id.substr(kPrefix.size(), firstColon - kPrefix.size());
    const std::string pidHex =
        secondColon == std::string::npos ? id.substr(firstColon + 1) : id.substr(firstColon + 1, secondColon - firstColon - 1);
    if (vidHex.empty() || pidHex.empty()) {
        return false;
    }
    try {
        vid = std::stoi(vidHex, nullptr, 16);
        pid = std::stoi(pidHex, nullptr, 16);
    } catch (const std::exception&) {
        return false;
    }
    serial = secondColon == std::string::npos ? std::string() : id.substr(secondColon + 1);
    return true;
}

CameraDescriptor describeDevice(const CameraId& id, const uvc_device_descriptor_t& desc) {
    CameraDescriptor descriptor;
    descriptor.id = id;
    descriptor.displayName = desc.product != nullptr ? desc.product : "UVC Camera";
    descriptor.backendId = kBackendId;
    if (desc.serialNumber != nullptr && desc.serialNumber[0] != '\0') {
        descriptor.serial = desc.serialNumber;
    }
    return descriptor;
}

}  // namespace

std::string UvcCameraBackend::id() const { return kBackendId; }

std::vector<CameraDescriptor> UvcCameraBackend::enumerate() {
    std::vector<CameraDescriptor> result;

    uvc_context_t* ctx = nullptr;
    if (uvc_init(&ctx, nullptr) != UVC_SUCCESS) {
        return result;
    }

    uvc_device_t** list = nullptr;
    if (uvc_get_device_list(ctx, &list) != UVC_SUCCESS) {
        uvc_exit(ctx);
        return result;
    }

    for (std::size_t i = 0; list[i] != nullptr; ++i) {
        uvc_device_descriptor_t* desc = nullptr;
        if (uvc_get_device_descriptor(list[i], &desc) != UVC_SUCCESS) {
            continue;
        }

        // Opening briefly to inspect formats also means a device another
        // application already has open (e.g. oaCapture mid-capture) simply
        // fails here and is skipped — enumerate() never forces another
        // client off the device.
        uvc_device_handle_t* handle = nullptr;
        if (uvc_open(list[i], &handle) == UVC_SUCCESS) {
            if (bestRawFormat(uvc_get_format_descs(handle))) {
                const auto candidateId = makeId(desc->idVendor, desc->idProduct, desc->serialNumber);
                result.push_back(describeDevice(candidateId, *desc));
            }
            uvc_close(handle);
        }

        uvc_free_device_descriptor(desc);
    }

    uvc_free_device_list(list, 1);
    uvc_exit(ctx);
    return result;
}

std::unique_ptr<ICameraDevice> UvcCameraBackend::open(const CameraId& id) {
    int vid = 0, pid = 0;
    std::string serial;
    if (!parseId(id, vid, pid, serial)) {
        return nullptr;
    }

    uvc_context_t* ctx = nullptr;
    if (uvc_init(&ctx, nullptr) != UVC_SUCCESS) {
        return nullptr;
    }

    uvc_device_t* dev = nullptr;
    if (uvc_find_device(ctx, &dev, vid, pid, serial.empty() ? nullptr : serial.c_str()) != UVC_SUCCESS) {
        uvc_exit(ctx);
        return nullptr;
    }

    uvc_device_handle_t* handle = nullptr;
    if (uvc_open(dev, &handle) != UVC_SUCCESS) {
        uvc_unref_device(dev);
        uvc_exit(ctx);
        return nullptr;
    }

    uvc_device_descriptor_t* rawDesc = nullptr;
    CameraDescriptor descriptor;
    if (uvc_get_device_descriptor(dev, &rawDesc) == UVC_SUCCESS) {
        descriptor = describeDevice(id, *rawDesc);
        uvc_free_device_descriptor(rawDesc);
    } else {
        descriptor.id = id;
        descriptor.displayName = "UVC Camera";
        descriptor.backendId = kBackendId;
    }

    // create() takes ownership of ctx/dev/handle on every path, including
    // failure (no raw format available) — it closes/frees them itself.
    return UvcCameraDevice::create(std::move(descriptor), ctx, dev, handle);
}

}  // namespace astro::driver::uvc
