include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(gps_pipeline_tests
    test_placeholder.cpp
    test_types.cpp
    test_checksum.cpp
    test_parser.cpp
    test_filters.cpp
    test_output.cpp
    test_pipeline.cpp
    test_config.cpp
    test_fileoutput.cpp
    test_binary_parser.cpp
    test_record_readers.cpp
)

target_link_libraries(gps_pipeline_tests
    PRIVATE
        gps_pipeline_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(gps_pipeline_tests)
