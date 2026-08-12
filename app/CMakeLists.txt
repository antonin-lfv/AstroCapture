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
    MACOSX_BUNDLE
    main.cpp
)

# A bare Mach-O binary has no identity macOS's TCC privacy subsystem can
# register a Camera decision against, so raw USB Video-class access (the
# UVC driver) is silently refused and the app never even appears under
# System Settings > Privacy & Security > Camera to grant it manually — see
# docs/decisions/ADR-005-macos-camera-permission.md. A real .app bundle
# with NSCameraUsageDescription plus one AVFoundation authorization request
# (CameraPermission.mm) is what gets it a proper entry and prompt.
if(APPLE)
    set_target_properties(AstroCapture PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.astrocapture.AstroCapture"
        MACOSX_BUNDLE_BUNDLE_NAME "AstroCapture"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

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
        qml/HelpIcon.qml
    RESOURCES
        qml/assets/logo.png
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
        src/ZoomLoupeItem.hpp
        src/ZoomLoupeItem.cpp
        src/FileNaming.hpp
        src/FileNaming.cpp
)

target_include_directories(AstroCapture PRIVATE src)

target_link_libraries(AstroCapture PRIVATE Qt6::Quick Qt6::Gui astro::processing astro::recording Threads::Threads)

if(APPLE)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
    target_sources(AstroCapture PRIVATE src/CameraPermission.hpp src/CameraPermission.mm)
    target_link_libraries(AstroCapture PRIVATE ${AVFOUNDATION_FRAMEWORK})
else()
    target_sources(AstroCapture PRIVATE src/CameraPermission.hpp src/CameraPermission.cpp)
endif()

if(ASTRO_ENABLE_MOCK_CAMERA)
    target_link_libraries(AstroCapture PRIVATE astro::driver::mock)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_MOCK_CAMERA)
endif()

if(ASTRO_ENABLE_DRIVER_UVC)
    target_link_libraries(AstroCapture PRIVATE astro::driver::uvc)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_DRIVER_UVC)
endif()

if(ASTRO_ENABLE_DRIVER_EUVC)
    target_link_libraries(AstroCapture PRIVATE astro::driver::euvc)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_DRIVER_EUVC)
endif()

if(ASTRO_ENABLE_DRIVER_ZWO)
    target_link_libraries(AstroCapture PRIVATE astro::driver::zwo)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_DRIVER_ZWO)
    # ASICamera2 is a shared (not static) imported library — the built app
    # needs an explicit rpath to find it at load time in the build tree
    # (no install step exists in this project's workflow).
    set_target_properties(AstroCapture PROPERTIES BUILD_RPATH "${CMAKE_BINARY_DIR}/drivers/zwo")
endif()

if(APPLE)
    # Ad-hoc signing (identity "-") needs no Apple Developer account and is
    # only for a stable local TCC identity across rebuilds — an unsigned
    # bundle can get treated as a "new" app (re-prompting or losing its
    # Camera decision) every time it's rebuilt. Not a substitute for real
    # signing/notarization before distributing this to anyone else.
    add_custom_command(TARGET AstroCapture POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:AstroCapture>"
        COMMENT "Ad-hoc codesigning AstroCapture.app for a stable local TCC identity"
    )
endif()

