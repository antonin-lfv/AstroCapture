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

if(ASTRO_ENABLE_MOCK_CAMERA)
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
