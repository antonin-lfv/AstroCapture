cmake_minimum_required(VERSION 3.25)

project(AstroCapture VERSION 0.1.0 LANGUAGES CXX)

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

option(ASTRO_BUILD_TESTS "Build tests" ON)
option(ASTRO_ENABLE_DEVTOOLS "Enable developer tools" OFF)
option(ASTRO_ENABLE_MOCK_CAMERA "Build mock camera backend" ON)

if(ASTRO_BUILD_TESTS)
    enable_testing()
    include(AstroTesting)
endif()

add_subdirectory(core/domain)

if(ASTRO_ENABLE_MOCK_CAMERA)
    add_subdirectory(drivers/mock)
endif()

add_subdirectory(app)

if(ASTRO_BUILD_TESTS)
    add_subdirectory(tests/unit)
endif()
