add_library(astro_recording STATIC
    src/SerReader.cpp
    src/RawSequenceRecorder.cpp
    src/RawSequenceReader.cpp
    src/TiffWriter.cpp
    src/FitsWriter.cpp
    src/AstroImage.cpp
    src/FitsSequenceRecorder.cpp
    src/SerRecorder.cpp
)
add_library(astro::recording ALIAS astro_recording)

target_include_directories(astro_recording PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(astro_recording PUBLIC astro::domain)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CFITSIO QUIET IMPORTED_TARGET GLOBAL cfitsio)
    pkg_check_modules(LIBRAW QUIET IMPORTED_TARGET GLOBAL libraw)
endif()
if(TARGET PkgConfig::CFITSIO)
    add_library(astro_cfitsio ALIAS PkgConfig::CFITSIO)
else()
    find_path(CFITSIO_INCLUDE_DIR fitsio.h REQUIRED)
    find_library(CFITSIO_LIBRARY NAMES cfitsio REQUIRED)
    add_library(astro_cfitsio UNKNOWN IMPORTED GLOBAL)
    set_target_properties(astro_cfitsio PROPERTIES IMPORTED_LOCATION "${CFITSIO_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${CFITSIO_INCLUDE_DIR}")
endif()
if(TARGET PkgConfig::LIBRAW)
    add_library(astro_libraw ALIAS PkgConfig::LIBRAW)
else()
    find_path(LIBRAW_INCLUDE_DIR libraw/libraw.h REQUIRED)
    find_library(LIBRAW_LIBRARY NAMES raw libraw REQUIRED)
    add_library(astro_libraw UNKNOWN IMPORTED GLOBAL)
    set_target_properties(astro_libraw PROPERTIES IMPORTED_LOCATION "${LIBRAW_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${LIBRAW_INCLUDE_DIR}")
endif()
target_link_libraries(astro_recording PRIVATE astro_cfitsio astro_libraw)
