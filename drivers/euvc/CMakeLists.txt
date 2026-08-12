find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
find_package(Threads REQUIRED)

add_library(astro_driver_euvc STATIC
    src/EuvcCameraBackend.cpp
    src/EuvcCameraDevice.cpp
)
add_library(astro::driver::euvc ALIAS astro_driver_euvc)

target_include_directories(astro_driver_euvc
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(astro_driver_euvc
    PUBLIC astro::domain
    PRIVATE PkgConfig::LIBUSB Threads::Threads
)
