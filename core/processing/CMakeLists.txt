find_package(Threads REQUIRED)

add_library(astro_processing STATIC
    src/Histogram.cpp
    src/Debayer.cpp
)
add_library(astro::processing ALIAS astro_processing)

target_include_directories(astro_processing PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(astro_processing PUBLIC astro::domain PRIVATE Threads::Threads)
