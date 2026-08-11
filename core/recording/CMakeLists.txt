add_library(astro_recording STATIC
    src/RawSequenceRecorder.cpp
    src/RawSequenceReader.cpp
    src/TiffWriter.cpp
    src/FitsWriter.cpp
)
add_library(astro::recording ALIAS astro_recording)

target_include_directories(astro_recording PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(astro_recording PUBLIC astro::domain)
