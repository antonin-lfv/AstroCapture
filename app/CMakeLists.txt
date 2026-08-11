find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui)
find_package(Threads REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

# QTP0004 doesn't exist before the Qt version that introduced it; guard so
# the minimum supported Qt 6.5 still configures (this is Qt's documented
# forward-compatible pattern for opting into policies).
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(AstroCapture
    main.cpp
)

qt_add_qml_module(AstroCapture
    URI AstroCapture
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/SettingsPanel.qml
        qml/DarkSpinBox.qml
        qml/SidebarSection.qml
    SOURCES
        src/PreviewController.hpp
        src/PreviewController.cpp
        src/LivePreviewItem.hpp
        src/LivePreviewItem.cpp
        src/FramePreviewImage.hpp
        src/FramePreviewImage.cpp
        src/Settings.hpp
        src/Settings.cpp
        src/HistogramItem.hpp
        src/HistogramItem.cpp
        src/FileNaming.hpp
        src/FileNaming.cpp
)

target_include_directories(AstroCapture PRIVATE src)

target_link_libraries(AstroCapture PRIVATE Qt6::Quick Qt6::Gui astro::processing astro::recording Threads::Threads)

if(ASTRO_ENABLE_MOCK_CAMERA)
    target_link_libraries(AstroCapture PRIVATE astro::driver::mock)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_MOCK_CAMERA)
endif()

if(ASTRO_ENABLE_DRIVER_UVC AND APPLE)
    target_link_libraries(AstroCapture PRIVATE astro::driver::uvc)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_DRIVER_UVC)
endif()
