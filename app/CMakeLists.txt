find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(AstroCapture
    main.cpp
)

qt_add_qml_module(AstroCapture
    URI AstroCapture
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/PreviewController.hpp
        src/PreviewController.cpp
        src/LivePreviewItem.hpp
        src/LivePreviewItem.cpp
        src/SimpleDebayer.hpp
        src/SimpleDebayer.cpp
)

target_include_directories(AstroCapture PRIVATE src)

target_link_libraries(AstroCapture PRIVATE Qt6::Quick Qt6::Gui)

if(ASTRO_ENABLE_MOCK_CAMERA)
    target_link_libraries(AstroCapture PRIVATE astro::driver::mock)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_MOCK_CAMERA)
endif()
