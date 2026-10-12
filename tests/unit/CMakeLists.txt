find_package(Threads REQUIRED)

astro_add_unit_test(astro_domain_frame_descriptor_test
    SOURCES domain/FrameDescriptorTest.cpp
    LIBS astro::domain
)

astro_add_unit_test(astro_domain_result_test
    SOURCES domain/ResultTest.cpp
    LIBS astro::domain
)

astro_add_unit_test(astro_domain_latest_frame_slot_test
    SOURCES domain/LatestFrameSlotTest.cpp
    LIBS astro::domain Threads::Threads
)

astro_add_unit_test(astro_domain_bounded_frame_queue_test
    SOURCES domain/BoundedFrameQueueTest.cpp
    LIBS astro::domain Threads::Threads
)

astro_add_unit_test(astro_processing_histogram_test
    SOURCES processing/HistogramTest.cpp
    LIBS astro::processing
)

astro_add_unit_test(astro_recording_raw_sequence_test
    SOURCES recording/RawSequenceRecorderTest.cpp
    LIBS astro::recording
)

astro_add_unit_test(astro_recording_tiff_writer_test
    SOURCES recording/TiffWriterTest.cpp
    LIBS astro::recording
)

astro_add_unit_test(astro_recording_fits_writer_test
    SOURCES recording/FitsWriterTest.cpp
    LIBS astro::recording
)

astro_add_unit_test(astro_recording_ser_test
    SOURCES recording/SerRecorderTest.cpp
    LIBS astro::recording
)

if(ASTRO_ENABLE_MOCK_CAMERA)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui)
    astro_add_unit_test(astro_app_recording_lifecycle_test
        SOURCES app/RecordingLifecycleTest.cpp
            ../../app/src/PreviewController.cpp ../../app/src/PreviewController.hpp
            ../../app/src/Settings.cpp ../../app/src/FileNaming.cpp ../../app/src/FramePreviewImage.cpp
        LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording astro::driver::mock
    )
    set_target_properties(astro_app_recording_lifecycle_test PROPERTIES AUTOMOC ON)
    target_include_directories(astro_app_recording_lifecycle_test PRIVATE ../../app/src)
    target_compile_definitions(astro_app_recording_lifecycle_test PRIVATE ASTRO_ENABLE_MOCK_CAMERA)
    set_tests_properties(astro_app_recording_lifecycle_test PROPERTIES TIMEOUT 20)

    astro_add_unit_test(astro_driver_mock_bayer_test_pattern_test
        SOURCES mock/BayerTestPatternTest.cpp
        LIBS astro::driver::mock
    )

    astro_add_unit_test(astro_driver_mock_camera_device_test
        SOURCES mock/MockCameraDeviceTest.cpp
        LIBS astro::driver::mock
    )

    # Uses drivers/mock's synthetic Bayer generator to produce the known
    # color-bar mosaic astro_processing's debayer is checked against;
    # gated the same way since it needs that generator.
    astro_add_unit_test(astro_processing_debayer_test
        SOURCES processing/DebayerTest.cpp
        LIBS astro::processing astro::driver::mock
    )
endif()

astro_add_unit_test(astro_recording_ser_reader_test
    SOURCES recording/SerReaderTest.cpp
    LIBS astro::recording
)
astro_add_unit_test(astro_processing_planetary_test
    SOURCES processing/PlanetaryTest.cpp
    LIBS astro::processing
)
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui Network)
astro_add_unit_test(astro_app_processing_lifecycle_test
    SOURCES app/ProcessingLifecycleTest.cpp
        ../../app/src/ProcessingController.cpp ../../app/src/ProcessingController.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_processing_lifecycle_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_processing_lifecycle_test PRIVATE ../../app/src)
set_tests_properties(astro_app_processing_lifecycle_test PROPERTIES TIMEOUT 30)

astro_add_unit_test(astro_app_library_test
    SOURCES app/AppLibraryTest.cpp ../../app/src/AppLibrary.cpp ../../app/src/AppLibrary.hpp
    LIBS Qt6::Quick Qt6::Gui
)
set_target_properties(astro_app_library_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_library_test PRIVATE ../../app/src)

astro_add_unit_test(astro_app_history_drop_test
    SOURCES app/HistoryDropTest.cpp ../../app/src/AppLibrary.cpp ../../app/src/AppLibrary.hpp
    LIBS Qt6::Quick Qt6::Gui
)
set_target_properties(astro_app_history_drop_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_history_drop_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_history_drop_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(astro_app_history_drop_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

astro_add_unit_test(astro_app_processing_edits_test
    SOURCES app/ProcessingEditsTest.cpp ../../app/src/ProcessingEdits.cpp ../../app/src/ProcessingEdits.hpp
    LIBS Qt6::Quick Qt6::Gui
)
set_target_properties(astro_app_processing_edits_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_processing_edits_test PRIVATE ../../app/src)

astro_add_unit_test(astro_app_processing_workspace_test
    SOURCES app/ProcessingWorkspaceTest.cpp
        ../../app/src/ProcessingController.cpp ../../app/src/ProcessingController.hpp
        ../../app/src/ProcessingEdits.cpp ../../app/src/ProcessingEdits.hpp
        ../../app/src/ProcessingImageItem.cpp ../../app/src/ProcessingImageItem.hpp
        ../../app/src/AppLibrary.cpp ../../app/src/AppLibrary.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_processing_workspace_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_processing_workspace_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_processing_workspace_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(astro_app_processing_workspace_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)

astro_add_unit_test(astro_processing_vision_test
    SOURCES processing/VisionTest.cpp
    LIBS astro::processing
)
set_tests_properties(astro_processing_vision_test PROPERTIES TIMEOUT 120)
astro_add_unit_test(astro_app_mosaic_lifecycle_test
    SOURCES app/MosaicLifecycleTest.cpp ../../app/src/MosaicController.cpp ../../app/src/MosaicController.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_mosaic_lifecycle_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_mosaic_lifecycle_test PRIVATE ../../app/src)
set_tests_properties(astro_app_mosaic_lifecycle_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)

astro_add_unit_test(astro_app_mosaic_drop_test
    SOURCES app/MosaicDropTest.cpp ../../app/src/MosaicController.cpp ../../app/src/MosaicController.hpp
        ../../app/src/MosaicImageItem.hpp ../../app/src/AppLibrary.cpp ../../app/src/AppLibrary.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_mosaic_drop_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_mosaic_drop_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_mosaic_drop_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(astro_app_mosaic_drop_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)

astro_add_unit_test(astro_recording_astro_image_test
    SOURCES recording/AstroImageTest.cpp
    LIBS astro::recording astro_cfitsio
)
astro_add_unit_test(astro_processing_deep_sky_test
    SOURCES processing/DeepSkyTest.cpp
    LIBS astro::processing
)
set_tests_properties(astro_processing_deep_sky_test PROPERTIES TIMEOUT 120)
astro_add_unit_test(astro_processing_deep_sky_advanced_test
    SOURCES processing/DeepSkyAdvancedTest.cpp
    LIBS astro::processing
)
astro_add_unit_test(astro_processing_astrometry_test SOURCES processing/AstrometryTest.cpp LIBS astro::processing)
set_tests_properties(astro_processing_astrometry_test PROPERTIES TIMEOUT 90)
set_tests_properties(astro_processing_deep_sky_advanced_test PROPERTIES TIMEOUT 120)
astro_add_unit_test(astro_app_deep_sky_workspace_test
    SOURCES app/DeepSkyWorkspaceTest.cpp
        ../../app/src/DeepSkyEdits.cpp ../../app/src/DeepSkyEdits.hpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/DeepSkyImageItem.hpp ../../app/src/AppLibrary.cpp ../../app/src/AppLibrary.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_deep_sky_workspace_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_deep_sky_workspace_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_deep_sky_workspace_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(astro_app_deep_sky_workspace_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)

# Manual performance measurements; timings must not become CI pass thresholds.
add_executable(astro_processing_benchmark ../benchmarks/ProcessingBenchmark.cpp)
target_link_libraries(astro_processing_benchmark PRIVATE astro::processing astro::recording)
astro_add_unit_test(astro_processing_metal_test
    SOURCES processing/MetalProcessingTest.cpp
    LIBS astro::processing Threads::Threads
)
set_tests_properties(astro_processing_metal_test PROPERTIES TIMEOUT 60)

astro_add_unit_test(astro_app_deep_sky_edits_test
    SOURCES app/DeepSkyEditsTest.cpp ../../app/src/DeepSkyEdits.cpp ../../app/src/DeepSkyEdits.hpp
    LIBS Qt6::Quick Qt6::Gui
)
set_target_properties(astro_app_deep_sky_edits_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_deep_sky_edits_test PRIVATE ../../app/src)

astro_add_unit_test(astro_app_studio_workspace_test
    SOURCES app/StudioWorkspaceTest.cpp ../../app/src/AppLibrary.cpp ../../app/src/AppLibrary.hpp
    LIBS Qt6::Quick Qt6::Gui
)
set_target_properties(astro_app_studio_workspace_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_studio_workspace_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_studio_workspace_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(astro_app_studio_workspace_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)

foreach(workspace_test IN ITEMS astro_app_processing_workspace_test astro_app_deep_sky_workspace_test astro_app_mosaic_drop_test)
    target_sources(${workspace_test} PRIVATE ../../app/src/ExportSizeEstimator.cpp ../../app/src/ExportSizeEstimator.hpp)
endforeach()
astro_add_unit_test(astro_app_export_size_estimator_test
    SOURCES app/ExportSizeEstimatorTest.cpp ../../app/src/ExportSizeEstimator.cpp ../../app/src/ExportSizeEstimator.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing
)
set_target_properties(astro_app_export_size_estimator_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_export_size_estimator_test PRIVATE ../../app/src)
set_tests_properties(astro_app_export_size_estimator_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

astro_add_unit_test(astro_app_session_test
    SOURCES app/SessionTest.cpp
        ../../app/src/SessionController.cpp ../../app/src/SessionController.hpp
        ../../app/src/SessionStorage.cpp ../../app/src/WorkspaceSessions.cpp
        ../../app/src/ProcessingController.cpp ../../app/src/ProcessingController.hpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/MosaicController.cpp ../../app/src/MosaicController.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_session_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_session_test PRIVATE ../../app/src)
set_tests_properties(astro_app_session_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 45)
astro_add_unit_test(astro_processing_geometry_test SOURCES processing/GeometryTest.cpp LIBS astro::processing)
foreach(workspace_test IN ITEMS astro_app_processing_workspace_test astro_app_deep_sky_workspace_test)
    target_sources(${workspace_test} PRIVATE ../../app/src/ImagePixelsItem.hpp)
endforeach()

target_sources(astro_app_deep_sky_workspace_test PRIVATE ../../app/src/MaskOverlayItem.hpp)
astro_add_unit_test(astro_processing_local_mask_test SOURCES processing/LocalMaskTest.cpp LIBS astro::processing)
astro_add_unit_test(astro_app_scientific_editing_test
    SOURCES app/ScientificEditingTest.cpp ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/ProcessingController.cpp ../../app/src/ProcessingController.hpp
        ../../app/src/MosaicController.cpp ../../app/src/MosaicController.hpp ../../app/src/WorkspaceSessions.cpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_scientific_editing_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_scientific_editing_test PRIVATE ../../app/src)
set_tests_properties(astro_app_scientific_editing_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 45)

target_link_libraries(astro_app_scientific_editing_test PRIVATE astro_cfitsio)

foreach(project_test IN ITEMS astro_app_deep_sky_workspace_test astro_app_session_test astro_app_scientific_editing_test)
    target_sources(${project_test} PRIVATE ../../app/src/DeepSkySession.cpp ../../app/src/ProjectStorage.cpp ../../app/src/SessionStorage.cpp)
endforeach()
astro_add_unit_test(astro_app_portable_project_test
    SOURCES app/PortableProjectTest.cpp
        ../../app/src/ProjectStorage.cpp ../../app/src/SessionStorage.cpp
        ../../app/src/DeepSkySession.cpp ../../app/src/WorkspaceSessions.cpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/ProcessingController.cpp ../../app/src/ProcessingController.hpp
        ../../app/src/MosaicController.cpp ../../app/src/MosaicController.hpp
        ../../app/src/SessionController.cpp ../../app/src/SessionController.hpp
    LIBS Qt6::Quick Qt6::Gui astro::processing astro::recording
)
set_target_properties(astro_app_portable_project_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_portable_project_test PRIVATE ../../app/src)
set_tests_properties(astro_app_portable_project_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 60)

foreach(astrometry_test IN ITEMS astro_app_deep_sky_workspace_test astro_app_session_test astro_app_scientific_editing_test astro_app_portable_project_test)
    target_sources(${astrometry_test} PRIVATE ../../app/src/DeepSkyAstrometry.cpp ../../app/src/BlindSolver.cpp ../../app/src/GaiaCatalog.cpp ../../app/src/TargetResolver.cpp)
    target_link_libraries(${astrometry_test} PRIVATE Qt6::Network ZLIB::ZLIB)
endforeach()

astro_add_unit_test(astro_app_gaia_catalog_test SOURCES app/GaiaCatalogTest.cpp ../../app/src/GaiaCatalog.cpp LIBS Qt6::Core astro::processing ZLIB::ZLIB)
target_include_directories(astro_app_gaia_catalog_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_gaia_catalog_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
astro_add_unit_test(astro_app_target_resolver_test SOURCES app/TargetResolverTest.cpp ../../app/src/TargetResolver.cpp LIBS Qt6::Core Qt6::Network)
target_include_directories(astro_app_target_resolver_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_target_resolver_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

astro_add_unit_test(astro_processing_sip_wcs_test SOURCES processing/SipWcsTest.cpp LIBS astro::recording)
astro_add_unit_test(astro_app_blind_solver_test
    SOURCES app/BlindSolverTest.cpp ../../app/src/BlindSolver.cpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/DeepSkyAstrometry.cpp ../../app/src/GaiaCatalog.cpp ../../app/src/TargetResolver.cpp
        ../../app/src/DeepSkySession.cpp ../../app/src/ProjectStorage.cpp ../../app/src/SessionStorage.cpp
    LIBS Qt6::Quick Qt6::Gui Qt6::Network astro::processing astro::recording ZLIB::ZLIB)
set_target_properties(astro_app_blind_solver_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_blind_solver_test PRIVATE ../../app/src)
set_tests_properties(astro_app_blind_solver_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 210)

foreach(master_target IN ITEMS astro_app_deep_sky_workspace_test astro_app_session_test astro_app_scientific_editing_test astro_app_portable_project_test astro_app_blind_solver_test)
    target_sources(${master_target} PRIVATE ../../app/src/MasterLibrary.cpp)
endforeach()
astro_add_unit_test(astro_app_master_library_test
    SOURCES app/MasterLibraryTest.cpp ../../app/src/MasterLibrary.cpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/DeepSkyAstrometry.cpp ../../app/src/BlindSolver.cpp ../../app/src/GaiaCatalog.cpp ../../app/src/TargetResolver.cpp
        ../../app/src/DeepSkySession.cpp ../../app/src/ProjectStorage.cpp ../../app/src/SessionStorage.cpp
    LIBS Qt6::Quick Qt6::Gui Qt6::Network astro::processing astro::recording ZLIB::ZLIB)
set_target_properties(astro_app_master_library_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_master_library_test PRIVATE ../../app/src)
set_tests_properties(astro_app_master_library_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
astro_add_unit_test(astro_app_batch_master_library_test
    SOURCES app/BatchMasterLibraryTest.cpp ../../app/src/MasterLibrary.cpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp
        ../../app/src/DeepSkyAstrometry.cpp ../../app/src/BlindSolver.cpp ../../app/src/GaiaCatalog.cpp ../../app/src/TargetResolver.cpp
        ../../app/src/DeepSkySession.cpp ../../app/src/ProjectStorage.cpp ../../app/src/SessionStorage.cpp
    LIBS Qt6::Quick Qt6::Gui Qt6::Network astro::processing astro::recording ZLIB::ZLIB)
set_target_properties(astro_app_batch_master_library_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_batch_master_library_test PRIVATE ../../app/src)
set_tests_properties(astro_app_batch_master_library_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
astro_add_unit_test(astro_processing_star_layers_test SOURCES processing/StarLayersTest.cpp LIBS astro::processing)
astro_add_unit_test(astro_app_star_layers_test
    SOURCES app/StarLayersControllerTest.cpp ../../app/src/MasterLibrary.cpp
        ../../app/src/DeepSkyController.cpp ../../app/src/DeepSkyController.hpp ../../app/src/DeepSkyEdits.cpp ../../app/src/DeepSkyEdits.hpp
        ../../app/src/DeepSkyAstrometry.cpp ../../app/src/BlindSolver.cpp ../../app/src/GaiaCatalog.cpp ../../app/src/TargetResolver.cpp
        ../../app/src/DeepSkySession.cpp ../../app/src/ProjectStorage.cpp ../../app/src/SessionStorage.cpp
    LIBS Qt6::Quick Qt6::Gui Qt6::Network astro::processing astro::recording ZLIB::ZLIB)
set_target_properties(astro_app_star_layers_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_star_layers_test PRIVATE ../../app/src)
set_tests_properties(astro_app_star_layers_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)

astro_add_unit_test(astro_processing_drizzle_test
    SOURCES processing/DrizzleTest.cpp
    LIBS astro::processing
)

astro_add_unit_test(astro_processing_integration_diagnostics_test
    SOURCES processing/IntegrationDiagnosticsTest.cpp
    LIBS astro::processing
)

astro_add_unit_test(astro_processing_integration_resources_test
    SOURCES processing/IntegrationResourcesTest.cpp
    LIBS astro::processing
)

astro_add_unit_test(astro_processing_wcs_projection_test
    SOURCES processing/WcsProjectionTest.cpp
    LIBS astro::processing
)

# Exercise the complete window and its real signal connections in separate
# processes. Only the mock backend is compiled into this validation runner.
if(ASTRO_ENABLE_MOCK_CAMERA)
    get_target_property(personal_app_sources AstroCapture SOURCES)
    set(personal_controller_sources)
    foreach(source IN LISTS personal_app_sources)
        if(source MATCHES "^src/.*\\.(cpp|hpp)$" AND NOT source MATCHES "CameraPermission")
            list(APPEND personal_controller_sources "${PROJECT_SOURCE_DIR}/app/${source}")
        endif()
    endforeach()
    astro_add_unit_test(astro_app_personal_use_test
        SOURCES app/PersonalUseTest.cpp ${personal_controller_sources}
        LIBS Qt6::Quick Qt6::Gui Qt6::Network astro::processing astro::recording
            astro::driver::mock Threads::Threads ZLIB::ZLIB
    )
    set_target_properties(astro_app_personal_use_test PROPERTIES AUTOMOC ON)
    target_link_libraries(astro_app_personal_use_test PRIVATE astro_observation_ephemeris)
    qt_add_resources(astro_app_personal_use_test observation_data PREFIX "/observation" BASE "${PROJECT_SOURCE_DIR}/app/data"
        FILES ../../app/data/NGC.csv ../../app/data/NGC-addendum.csv ../../app/data/world-land.json)
    target_include_directories(astro_app_personal_use_test PRIVATE ../../app/src)
    target_compile_definitions(astro_app_personal_use_test PRIVATE
        ASTRO_ENABLE_MOCK_CAMERA ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        ASTRO_TRANSLATION_PATH="${PROJECT_BINARY_DIR}/app/astrocapture_fr.qm"
        ASTRO_APP_VERSION="${PROJECT_VERSION}"
    )
    set_tests_properties(astro_app_personal_use_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 120)
endif()

astro_add_unit_test(astro_app_observation_test
    SOURCES app/ObservationTest.cpp ../../app/src/ObservationAstronomy.cpp
        ../../app/src/ObservationController.cpp ../../app/src/ObservationController.hpp
        ../../app/src/ObservationWeather.cpp ../../app/src/ObservationWeather.hpp
        ../../app/src/ObservationPlot.cpp ../../app/src/ObservationPlot.hpp ../../app/src/TargetResolver.cpp
    LIBS Qt6::Quick Qt6::Gui Qt6::Network astro_observation_ephemeris Threads::Threads)
set_target_properties(astro_app_observation_test PROPERTIES AUTOMOC ON)
target_include_directories(astro_app_observation_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_observation_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}" ASTRO_TRANSLATION_PATH="${PROJECT_BINARY_DIR}/app/astrocapture_fr.qm")
qt_add_resources(astro_app_observation_test observation_data PREFIX "/observation" BASE "${PROJECT_SOURCE_DIR}/app/data"
    FILES ../../app/data/NGC.csv ../../app/data/NGC-addendum.csv ../../app/data/world-land.json)
set_tests_properties(astro_app_observation_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)

astro_add_unit_test(astro_app_observation_weather_test
    SOURCES app/ObservationWeatherTest.cpp ../../app/src/ObservationWeather.cpp
    LIBS Qt6::Core Qt6::Network)
target_include_directories(astro_app_observation_weather_test PRIVATE ../../app/src)
target_compile_definitions(astro_app_observation_weather_test PRIVATE ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# App settings and session validators share domain image bounds.
get_directory_property(astro_unit_targets BUILDSYSTEM_TARGETS)
foreach(unit_target IN LISTS astro_unit_targets)
    get_target_property(unit_type ${unit_target} TYPE)
    if(unit_target MATCHES "^astro_app_" AND NOT unit_type STREQUAL "UTILITY")
        target_link_libraries(${unit_target} PRIVATE astro::domain)
    endif()
endforeach()
