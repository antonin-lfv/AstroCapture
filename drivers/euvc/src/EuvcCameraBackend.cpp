#include "astro/driver/euvc/EuvcCameraBackend.hpp"

#include <iomanip>
#include <sstream>

#include <libusb.h>

#include "EuvcCameraDevice.hpp"
#include "EuvcProtocol.hpp"

namespace astro::driver::euvc {

using namespace astro::domain;
namespace proto = protocol;

namespace {

constexpr const char* kBackendId = "euvc";

std::string makeId(std::uint16_t vid, std::uint16_t pid, const std::string& serial) {
    std::ostringstream out;
    out << "euvc:" << std::hex << std::setfill('0') << std::setw(4) << vid << ':' << std::setw(4) << pid;
    if (!serial.empty()) {
        out << ':' << serial;
    }
    return out.str();
}

bool parseId(const CameraId& id, int& vid, int& pid, std::string& serial) {
    constexpr std::string_view kPrefix = "euvc:";
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

// True if this device's active configuration declares the vendor-specific
// control interface this camera family uses (class 0xFF, subclass 1) —
// the filter that keeps enumerate() from ever matching an unrelated
// device (ADR-006 / EuvcCameraBackend.hpp class comment).
bool hasExpectedVendorInterface(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(device, &config) != 0) {
        return false;
    }
    bool found = false;
    for (int i = 0; i < config->bNumInterfaces && !found; ++i) {
        const auto& iface = config->interface[i].altsetting[0];
        found = iface.bInterfaceClass == proto::kVendorInterfaceClass &&
                iface.bInterfaceSubClass == proto::kVendorInterfaceSubClass;
    }
    libusb_free_config_descriptor(config);
    return found;
}

std::string readSerial(libusb_device_handle* handle, const libusb_device_descriptor& desc) {
    if (desc.iSerialNumber == 0) {
        return {};
    }
    unsigned char buffer[256] = {0};
    const int len = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, buffer, sizeof(buffer));
    return len > 0 ? std::string(reinterpret_cast<char*>(buffer), static_cast<std::size_t>(len)) : std::string();
}

}  // namespace

std::string EuvcCameraBackend::id() const { return kBackendId; }

std::vector<CameraDescriptor> EuvcCameraBackend::enumerate() {
    std::vector<CameraDescriptor> result;

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        return result;
    }

    libusb_device** list = nullptr;
    const auto count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        libusb_exit(ctx);
        return result;
    }

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0 || desc.idVendor != proto::kTisVendorId) {
            continue;
        }
        if (!hasExpectedVendorInterface(list[i])) {
            continue;
        }

        libusb_device_handle* handle = nullptr;
        if (libusb_open(list[i], &handle) != 0) {
            // Another application already has it open, or access was
            // otherwise denied — skip rather than fail the whole scan.
            continue;
        }
        const std::string serial = readSerial(handle, desc);
        libusb_close(handle);

        CameraDescriptor descriptor;
        descriptor.id = makeId(desc.idVendor, desc.idProduct, serial);
        descriptor.displayName = "Celestron/TIS USB2 camera";
        descriptor.backendId = kBackendId;
        if (!serial.empty()) {
            descriptor.serial = serial;
        }
        result.push_back(std::move(descriptor));
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return result;
}

std::unique_ptr<ICameraDevice> EuvcCameraBackend::open(const CameraId& id) {
    int vid = 0, pid = 0;
    std::string serial;
    if (!parseId(id, vid, pid, serial)) {
        return nullptr;
    }

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        return nullptr;
    }

    libusb_device** list = nullptr;
    const auto count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        libusb_exit(ctx);
        return nullptr;
    }

    libusb_device* match = nullptr;
    for (ssize_t i = 0; i < count && match == nullptr; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != vid || desc.idProduct != pid) {
            continue;
        }
        if (serial.empty()) {
            match = list[i];
            break;
        }
        libusb_device_handle* probe = nullptr;
        if (libusb_open(list[i], &probe) == 0) {
            if (readSerial(probe, desc) == serial) {
                match = list[i];
            }
            libusb_close(probe);
        }
    }

    if (match == nullptr) {
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return nullptr;
    }

    libusb_device_handle* handle = nullptr;
    const int openErr = libusb_open(match, &handle);
    libusb_free_device_list(list, 1);
    if (openErr != 0) {
        libusb_exit(ctx);
        return nullptr;
    }

    CameraDescriptor descriptor;
    descriptor.id = id;
    descriptor.displayName = "Celestron/TIS USB2 camera";
    descriptor.backendId = kBackendId;
    if (!serial.empty()) {
        descriptor.serial = serial;
    }

    // create() takes ownership of ctx/handle on every path, including
    // failure (interfaces/format not found) — it closes/frees them itself.
    return EuvcCameraDevice::create(std::move(descriptor), ctx, handle);
}

}  // namespace astro::driver::euvc
