find_package(Threads REQUIRED)

add_library(astro_processing STATIC
    src/Planetary.cpp
    src/DeepSky.cpp
    src/Surface.cpp
    src/Mosaic.cpp
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
