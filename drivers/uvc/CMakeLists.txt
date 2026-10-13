add_library(astro_driver_uvc STATIC
    src/UvcCameraBackend.cpp
    src/UvcCameraDevice.cpp
    src/UvcFormat.cpp
)
add_library(astro::driver::uvc ALIAS astro_driver_uvc)

target_include_directories(astro_driver_uvc
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(astro_driver_uvc
    PUBLIC astro::domain
    PRIVATE ${ASTRO_UVC_TARGET}
)
