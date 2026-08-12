# Preconditions (APPLE, libusb found) are checked by the caller in the
# root CMakeLists.txt, same pattern as drivers/uvc and drivers/euvc.
set(ZWO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/zwo-sdk")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(ZWO_LIB_SUBDIR "mac_arm64")
else()
    set(ZWO_LIB_SUBDIR "mac_x64")
endif()
set(ZWO_LIB_SRC "${ZWO_SDK_DIR}/lib/${ZWO_LIB_SUBDIR}/libASICamera2.bin")

# libASICamera2.bin's embedded dependency is "@rpath/libusb-1.0.0.dylib"
# (confirmed via otool -L; see third_party/zwo-sdk/README.md), but
# Homebrew's libusb installs as libusb-1.0.dylib — no trailing ".0". A copy
# is patched at configure time (via install_name_tool) rather than
# consuming the vendored .bin directly, so third_party/zwo-sdk/ stays
# exactly what was fetched from upstream.
set(ZWO_LIB_PATCHED "${CMAKE_CURRENT_BINARY_DIR}/libASICamera2.dylib")
file(COPY_FILE "${ZWO_LIB_SRC}" "${ZWO_LIB_PATCHED}" ONLY_IF_DIFFERENT)
execute_process(COMMAND install_name_tool -id "@rpath/libASICamera2.dylib" "${ZWO_LIB_PATCHED}")
execute_process(COMMAND install_name_tool -change "@rpath/libusb-1.0.0.dylib" "${ZWO_LIBUSB_DYLIB}" "${ZWO_LIB_PATCHED}")
execute_process(COMMAND codesign --force --sign - "${ZWO_LIB_PATCHED}")

add_library(ASICamera2 SHARED IMPORTED GLOBAL)
set_target_properties(ASICamera2 PROPERTIES IMPORTED_LOCATION "${ZWO_LIB_PATCHED}")

add_library(astro_driver_zwo STATIC
    src/ZwoCameraBackend.cpp
    src/ZwoCameraDevice.cpp
)
add_library(astro::driver::zwo ALIAS astro_driver_zwo)

target_include_directories(astro_driver_zwo
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${ZWO_SDK_DIR}/include"
)

target_link_libraries(astro_driver_zwo
    PUBLIC astro::domain
    PRIVATE ASICamera2
)
