# Fixed, minimal, static OpenCV build. No camera/video I/O, codecs, network
# models or runtime downloads: Qt handles file loading and 16-bit TIFF export.
include(FetchContent)
set(BUILD_LIST core,imgproc,features2d,calib3d CACHE STRING "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
foreach(option BUILD_TESTS BUILD_PERF_TESTS BUILD_EXAMPLES BUILD_opencv_apps BUILD_JAVA BUILD_opencv_python2 BUILD_opencv_python3 BUILD_DOCS WITH_IPP WITH_OPENCL WITH_ITT WITH_FFMPEG WITH_GSTREAMER WITH_AVFOUNDATION WITH_MSMF WITH_V4L WITH_QT WITH_GTK WITH_PROTOBUF WITH_CUDA WITH_OPENEXR WITH_JASPER WITH_TIFF WITH_WEBP WITH_JPEG WITH_PNG WITH_1394)
    set(${option} OFF CACHE BOOL "" FORCE)
endforeach()
set(OPENCV_ENABLE_NONFREE OFF CACHE BOOL "" FORCE)
set(OPENCV_GENERATE_PKGCONFIG OFF CACHE BOOL "" FORCE)
FetchContent_Declare(astro_opencv
    URL https://github.com/opencv/opencv/archive/refs/tags/4.12.0.tar.gz
    URL_HASH SHA256=44c106d5bb47efec04e531fd93008b3fcd1d27138985c5baf4eafac0e1ec9e9d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(astro_opencv)

# Carry the upstream license and source-level notices into binary distributions.
set(ASTRO_VISION_NOTICES "${CMAKE_BINARY_DIR}/OpenCV-notices.txt")
file(READ "${astro_opencv_SOURCE_DIR}/LICENSE" vision_license)
file(WRITE "${ASTRO_VISION_NOTICES}" "OpenCV 4.12.0\nhttps://opencv.org/\n\n${vision_license}\n")
set(vision_notice_hashes "")
foreach(module core imgproc features2d flann calib3d)
    file(GLOB_RECURSE vision_sources "${astro_opencv_SOURCE_DIR}/modules/${module}/*.cpp" "${astro_opencv_SOURCE_DIR}/modules/${module}/*.hpp" "${astro_opencv_SOURCE_DIR}/modules/${module}/*.h")
    foreach(source IN LISTS vision_sources)
        file(READ "${source}" notice LIMIT 16000)
        string(REGEX MATCH "^[ \r\n]*[/][*]([^*]|[*]+[^*/])*[*]+/" notice "${notice}")
        if(notice MATCHES "[Ll]icense|[Cc]opyright")
            string(SHA256 notice_hash "${notice}")
            if(NOT notice_hash IN_LIST vision_notice_hashes)
                list(APPEND vision_notice_hashes "${notice_hash}")
                file(APPEND "${ASTRO_VISION_NOTICES}" "\n${notice}\n")
            endif()
        endif()
    endforeach()
endforeach()
foreach(source modules/core/3rdparty/SoftFloat/COPYING.txt modules/features2d/3rdparty/mscr/chi_table_LICENSE.txt hal/carotene/src/common.cpp 3rdparty/zlib/LICENSE)
    file(READ "${astro_opencv_SOURCE_DIR}/${source}" notice)
    if(source MATCHES "[.]cpp$")
        string(REGEX MATCH "^[ \r\n]*[/][*]([^*]|[*]+[^*/])*[*]+/" notice "${notice}")
    endif()
    file(APPEND "${ASTRO_VISION_NOTICES}" "\n${source}\n${notice}\n")
endforeach()
