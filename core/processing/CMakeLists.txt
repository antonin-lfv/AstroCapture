find_package(Threads REQUIRED)

add_library(astro_processing STATIC
    src/Planetary.cpp
    src/Geometry.cpp
    src/DeepSky.cpp
    src/Drizzle.cpp
    src/Hdr.cpp
    src/DeepSkyAdvanced.cpp
    src/StarLayers.cpp
    src/Astrometry.cpp
    src/Surface.cpp
    src/Mosaic.cpp
    src/Preview.cpp
    src/Histogram.cpp
    src/Debayer.cpp
)
add_library(astro::processing ALIAS astro_processing)

target_include_directories(astro_processing PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(astro_processing PUBLIC astro::domain astro::recording PRIVATE Threads::Threads opencv_core opencv_imgproc opencv_features2d opencv_calib3d)
# OpenCV's in-tree targets do not export their include directories.
target_include_directories(astro_processing PRIVATE
    "${astro_opencv_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}"
    "${astro_opencv_SOURCE_DIR}/modules/core/include"
    "${astro_opencv_SOURCE_DIR}/modules/imgproc/include"
    "${astro_opencv_SOURCE_DIR}/modules/features2d/include"
    "${astro_opencv_SOURCE_DIR}/modules/flann/include"
    "${astro_opencv_SOURCE_DIR}/modules/calib3d/include"
)

if(APPLE AND ASTRO_ENABLE_METAL)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/metal/Processing.metal" ASTRO_METAL_SHADER_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/metal/Processing.metal")
    configure_file(src/metal/MetalShaders.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/MetalShaders.hpp" @ONLY)
    target_include_directories(astro_processing PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    set_target_properties(astro_processing PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON OBJCXX_EXTENSIONS OFF)
    target_sources(astro_processing PRIVATE src/MetalProcessing.mm)
    set_source_files_properties(src/MetalProcessing.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(astro_processing PRIVATE "-framework Metal" "-framework Foundation")
else()
    target_sources(astro_processing PRIVATE src/MetalProcessingStub.cpp)
endif()
