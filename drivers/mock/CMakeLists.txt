find_package(Threads REQUIRED)

add_library(astro_driver_mock STATIC
    src/BayerTestPattern.cpp
    src/MockCameraBackend.cpp
    src/MockCameraDevice.cpp
)
add_library(astro::driver::mock ALIAS astro_driver_mock)

target_include_directories(astro_driver_mock
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(astro_driver_mock
    PUBLIC astro::domain
    PRIVATE Threads::Threads
)
