add_library(astro_domain STATIC
    src/PixelFormat.cpp
    src/Result.cpp
    src/ImageLimits.cpp
)
add_library(astro::domain ALIAS astro_domain)

target_include_directories(astro_domain PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(astro_domain PUBLIC cxx_std_20)
