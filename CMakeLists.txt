cmake_minimum_required(VERSION 3.25)

project(AstroCapture VERSION 1.20.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect all binaries under a single bin/ regardless of which subdirectory
# defines them. This also keeps executable output paths from colliding with
# same-named generated directories, such as a QML module's URI-named folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(APPLE)
    enable_language(OBJCXX)
endif()
option(ASTRO_ENABLE_METAL "Accelerate supported processing with Metal on Apple Silicon" ON)

option(ASTRO_BUILD_TESTS "Build tests" ON)
option(ASTRO_ENABLE_DEVTOOLS "Enable developer tools" OFF)
option(ASTRO_ENABLE_MOCK_CAMERA "Build mock camera backend" ON)
# Real-hardware backend for USB Video Class cameras (see
# docs/decisions/ADR-004-uvc-camera-backend.md). Needs libuvc (`brew install
# libuvc` on macOS); silently disabled if not found, so contributors without
# it installed still get a working build.
option(ASTRO_ENABLE_DRIVER_UVC "Build the libuvc-based real camera backend" ON)
# Proprietary-protocol backend for Celestron/TIS USB2 cameras in factory
# mode — see docs/decisions/ADR-006-euvc-proprietary-backend.md. Needs
# libusb (already a transitive dependency via libuvc, but pulled directly
# via pkg-config here since this backend never goes through libuvc itself).
option(ASTRO_ENABLE_DRIVER_EUVC "Build the proprietary-protocol TIS/NexImage camera backend" ON)
# ZWO ASI camera backend, wrapping ZWO's vendored ASICamera2 SDK — see
# docs/decisions/ADR-007-zwo-asi-backend.md. Vendored for macOS and Linux
# x86-64; needs libusb at runtime.
option(ASTRO_ENABLE_DRIVER_ZWO "Build the ZWO ASI camera backend" ON)

if(ASTRO_BUILD_TESTS)
    enable_testing()
    include(AstroTesting)
endif()

add_subdirectory(core/domain)
include(AstroVision)
if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlib)
        foreach(zlib_include IN LISTS ZLIB_INCLUDE_DIR)
            target_include_directories(zlib INTERFACE "$<BUILD_INTERFACE:${zlib_include}>")
        endforeach()
        add_library(ZLIB::ZLIB ALIAS zlib)
    else()
        find_package(ZLIB REQUIRED)
    endif()
endif()
add_subdirectory(core/recording)
add_subdirectory(core/processing)

if(ASTRO_ENABLE_MOCK_CAMERA)
    add_subdirectory(drivers/mock)
endif()

if(ASTRO_ENABLE_DRIVER_UVC)
    find_package(libuvc CONFIG QUIET)
    if(TARGET LibUVC::UVCStatic)
        set(ASTRO_UVC_TARGET LibUVC::UVCStatic)
    elseif(TARGET LibUVC::UVC)
        set(ASTRO_UVC_TARGET LibUVC::UVC)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(ASTRO_LIBUVC QUIET IMPORTED_TARGET libuvc)
            if(TARGET PkgConfig::ASTRO_LIBUVC)
                set(ASTRO_UVC_TARGET PkgConfig::ASTRO_LIBUVC)
            endif()
        endif()
    endif()
    if(ASTRO_UVC_TARGET)
        add_subdirectory(drivers/uvc)
    else()
        message(STATUS "libuvc not found — skipping the real UVC camera backend")
        set(ASTRO_ENABLE_DRIVER_UVC OFF)
    endif()
endif()

if(ASTRO_ENABLE_DRIVER_EUVC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(EUVC_LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
    endif()
    if(EUVC_LIBUSB_FOUND)
        add_subdirectory(drivers/euvc)
    else()
        message(STATUS "libusb-1.0 not found — skipping the proprietary TIS/NexImage camera backend")
        set(ASTRO_ENABLE_DRIVER_EUVC OFF)
    endif()
endif()

if(ASTRO_ENABLE_DRIVER_ZWO)
    if(APPLE)
        find_library(ZWO_LIBUSB_DYLIB NAMES usb-1.0 libusb-1.0 PATHS /opt/homebrew/lib /usr/local/lib)
        if(ZWO_LIBUSB_DYLIB)
            add_subdirectory(drivers/zwo)
        else()
            message(STATUS "libusb not found — skipping the ZWO ASI camera backend")
            set(ASTRO_ENABLE_DRIVER_ZWO OFF)
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(ZWO_LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
        endif()
        if(ZWO_LIBUSB_FOUND)
            add_subdirectory(drivers/zwo)
        else()
            message(STATUS "libusb not found — skipping the ZWO ASI camera backend")
            set(ASTRO_ENABLE_DRIVER_ZWO OFF)
        endif()
    else()
        message(STATUS "No vendored ZWO ASI library for this platform — skipping the backend")
        set(ASTRO_ENABLE_DRIVER_ZWO OFF)
    endif()
endif()

add_subdirectory(app)

if(ASTRO_BUILD_TESTS)
    add_subdirectory(tests/unit)
endif()
