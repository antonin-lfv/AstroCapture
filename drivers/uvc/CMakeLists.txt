find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
find_library(COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

add_library(astro_driver_uvc STATIC
    src/UvcCameraBackend.mm
    src/UvcCameraDevice.mm
)
add_library(astro::driver::uvc ALIAS astro_driver_uvc)

target_include_directories(astro_driver_uvc
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(astro_driver_uvc PRIVATE -fobjc-arc)

target_link_libraries(astro_driver_uvc
    PUBLIC astro::domain
    PRIVATE
        ${AVFOUNDATION_FRAMEWORK}
        ${COREMEDIA_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
)
