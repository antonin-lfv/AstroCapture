# Convert the existing logo to macOS's multi-resolution icon container.
# The generated files stay in the build tree; the PNG remains the source of truth.
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "GenerateMacIcon requires INPUT and OUTPUT paths")
endif()
find_program(SIPS_EXECUTABLE sips REQUIRED)
find_program(ICONUTIL_EXECUTABLE iconutil REQUIRED)
get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
set(iconset "${output_directory}/AstroCapture.iconset")
file(MAKE_DIRECTORY "${iconset}")

foreach(size IN ITEMS 16 32 128 256 512)
    foreach(scale IN ITEMS 1 2)
        math(EXPR pixels "${size} * ${scale}")
        set(suffix "")
        if(scale EQUAL 2)
            set(suffix "@2x")
        endif()
        execute_process(
            COMMAND "${SIPS_EXECUTABLE}" -z "${pixels}" "${pixels}" "${INPUT}"
                --out "${iconset}/icon_${size}x${size}${suffix}.png"
            RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Cannot generate ${pixels}px icon: ${error}")
        endif()
    endforeach()
endforeach()
execute_process(
    COMMAND "${ICONUTIL_EXECUTABLE}" -c icns "${iconset}" -o "${OUTPUT}"
    RESULT_VARIABLE result ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot assemble macOS icon: ${error}")
endif()
