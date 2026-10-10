find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui Network LinguistTools)
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

target_compile_definitions(AstroCapture PRIVATE ASTRO_APP_VERSION="${PROJECT_VERSION}")

# A bare Mach-O binary has no identity macOS's TCC privacy subsystem can
# register a Camera decision against, so raw USB Video-class access (the
# UVC driver) is silently refused and the app never even appears under
# System Settings > Privacy & Security > Camera to grant it manually — see
# docs/decisions/ADR-005-macos-camera-permission.md. A real .app bundle
# with NSCameraUsageDescription plus one AVFoundation authorization request
# (CameraPermission.mm) is what gets it a proper entry and prompt.
if(APPLE)
    set(app_icon "${CMAKE_CURRENT_BINARY_DIR}/AstroCapture.icns")
    add_custom_command(
        OUTPUT "${app_icon}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/qml/assets/logo.png"
            "-DOUTPUT=${app_icon}"
            -P "${PROJECT_SOURCE_DIR}/cmake/GenerateMacIcon.cmake"
        DEPENDS qml/assets/logo.png "${PROJECT_SOURCE_DIR}/cmake/GenerateMacIcon.cmake"
        COMMENT "Generating the macOS application icon from the logo"
        VERBATIM
    )
    set_source_files_properties("${app_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties("${ASTRO_VISION_NOTICES}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Licenses)
    file(GLOB astro_image_notices "${PROJECT_SOURCE_DIR}/third_party/notices/*")
    set_source_files_properties(${astro_image_notices} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Licenses)
    target_sources(AstroCapture PRIVATE "${app_icon}" "${ASTRO_VISION_NOTICES}" ${astro_image_notices})
    set_target_properties(AstroCapture PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "AstroCapture.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.astrocapture.AstroCapture"
        MACOSX_BUNDLE_BUNDLE_NAME "AstroCapture"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# This is a relative JavaScript library, not a public module type. Exporting
# it as a qmldir namespace as well makes the module's self-import ambiguous.
set_source_files_properties(qml/Atelier.js PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)

qt_add_qml_module(AstroCapture
    URI AstroCapture
    VERSION 1.0
    QML_FILES
        qml/Atelier.js
        qml/AtelierSlider.qml
        qml/InspectorFamily.qml
        qml/ParameterControl.qml
        qml/SettingLabel.qml
        qml/HelpCheckBox.qml
        qml/AtelierMenu.qml
        qml/AtelierMenuItem.qml
        qml/StudioNavigation.qml
        qml/HomeWorkspace.qml
        qml/Main.qml
        qml/ProcessingWorkspace.qml
        qml/DeepSkyWorkspace.qml
        qml/IntegrationTools.qml
        qml/DeepSkyAdvancedTools.qml
        qml/AstrometryDialog.qml
        qml/MasterLibraryDialog.qml
        qml/BatchMasterPane.qml
        qml/StarLayerTools.qml
        qml/MosaicWorkspace.qml
        qml/CaptionEditor.qml
        qml/ImageViewport.qml
        qml/EquipmentProfilesDialog.qml
    qml/GeometryEditor.qml
    qml/MaskEditor.qml
        qml/GlobalSettingsWorkspace.qml
        qml/HistoryWorkspace.qml
        qml/GuideWorkspace.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/IconAction.qml
        qml/ExportSizeHint.qml
        qml/SettingsPanel.qml
        qml/DarkComboBox.qml
        qml/DarkSpinBox.qml
        qml/SidebarSection.qml
        qml/HelpIcon.qml
        qml/DarkToolTip.qml
        qml/DarkDialog.qml
        qml/QuitConfirmation.qml
    RESOURCES
        qml/assets/logo.png
    SOURCES
        src/ImagePixelsItem.hpp
    src/MaskOverlayItem.hpp
    src/WorkspaceSnapshot.hpp
        src/WorkspaceSessions.cpp
        src/SessionStorage.hpp
        src/SessionStorage.cpp
        src/ProjectStorage.cpp src/ProjectStorage.hpp
        src/DeepSkySession.cpp
        src/DeepSkyAstrometry.cpp
        src/BlindSolver.cpp src/BlindSolver.hpp
        src/MasterLibrary.cpp src/MasterLibrary.hpp
        src/GaiaCatalog.cpp
        src/TargetResolver.cpp src/TargetResolver.hpp src/EquipmentProfile.hpp
        src/SessionController.hpp
        src/SessionController.cpp
        src/DeepSkyController.hpp
        src/DeepSkyController.cpp
        src/DeepSkyImageItem.hpp
        src/MosaicController.hpp
        src/MosaicController.cpp
        src/MosaicImageItem.hpp
        src/DeepSkyEdits.hpp
        src/DeepSkyEdits.cpp
        src/DeepSkySettings.hpp
        src/ProcessingEdits.hpp
        src/ProcessingEdits.cpp
        src/ProcessingSettings.hpp
        src/ExportAnnotation.hpp
        src/ExportSizeEstimator.hpp
        src/ExportSizeEstimator.cpp
        src/ProcessingController.hpp
        src/AppLibrary.hpp
        src/AppLibrary.cpp
        src/ProcessingController.cpp
        src/ProcessingImageItem.hpp
        src/ProcessingImageItem.cpp
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
if(ASTRO_ENABLE_DEVTOOLS)
    target_compile_definitions(AstroCapture PRIVATE ASTRO_ENABLE_DEVTOOLS)
endif()

target_link_libraries(AstroCapture PRIVATE Qt6::Quick Qt6::Gui Qt6::Network astro::processing astro::recording Threads::Threads ZLIB::ZLIB)

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
    # ASICamera2 is a shared (not static) imported library — CMake's
    # default automatic rpath computation already adds its directory for
    # us (it does this for any linked target with a known
    # IMPORTED_LOCATION), so nothing extra is needed here. Setting an
    # explicit BUILD_RPATH used to live here but replaces CMake's
    # automatic rpath list rather than adding to it, which silently
    # dropped Qt's own required rpath entries — invisible for a plain dev
    # run (Homebrew's Qt build uses absolute install names, not @rpath,
    # so it resolved anyway) but broke macdeployqt's packaging pass,
    # which specifically rewrites Qt's paths to @rpath ones and then
    # needs a correct rpath list to resolve them.
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

qt_add_translations(AstroCapture
    TS_FILES translations/astrocapture_fr.ts
    RESOURCE_PREFIX "/i18n"
)
