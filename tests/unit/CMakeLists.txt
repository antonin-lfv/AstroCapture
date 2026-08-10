astro_add_unit_test(astro_domain_frame_descriptor_test
    SOURCES domain/FrameDescriptorTest.cpp
    LIBS astro::domain
)

astro_add_unit_test(astro_domain_result_test
    SOURCES domain/ResultTest.cpp
    LIBS astro::domain
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
endif()
