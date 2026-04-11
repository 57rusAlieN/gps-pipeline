cmake_minimum_required(VERSION 3.14)

project(gps_pipeline CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure UTF-8 source and execution charset on MSVC (needed for degree symbol °)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Library: all business logic (parser, filters, output, pipeline)
# Populated incrementally as source files are added in later stages.
# ---------------------------------------------------------------------------
add_library(gps_pipeline_lib STATIC
    src/parser/ChecksumValidator.cpp
    src/parser/NmeaParser.cpp
    src/filter/SatelliteFilter.cpp
    src/filter/SpeedFilter.cpp
    src/filter/CoordinateJumpFilter.cpp
    src/filter/StopFilter.cpp
    src/filter/FilterChain.cpp
    src/filter/MovingAverageFilter.cpp
    src/filter/FirLowPassFilter.cpp
    src/output/ConsoleOutput.cpp
    src/pipeline/Pipeline.cpp
)

target_include_directories(gps_pipeline_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(gps_pipeline src/main.cpp)
target_link_libraries(gps_pipeline PRIVATE gps_pipeline_lib)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Code coverage (GCC / Clang only)
#   cmake --preset=linux-coverage && cmake --build --preset=linux-coverage
#   cmake --build --preset=linux-coverage --target coverage
# ---------------------------------------------------------------------------
option(ENABLE_COVERAGE "Build with gcov coverage instrumentation (GCC/Clang only)" OFF)

if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "ENABLE_COVERAGE requires GCC or Clang, got ${CMAKE_CXX_COMPILER_ID}")
    endif()

    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)

    find_program(LCOV    lcov   REQUIRED)
    find_program(GENHTML genhtml REQUIRED)

    add_custom_target(coverage
        COMMAND ${LCOV} --directory . --zerocounters
        COMMAND $<TARGET_FILE:gps_pipeline_tests>
        COMMAND ${LCOV} --directory . --capture
                        --output-file coverage.info
                        --exclude "*/googletest/*"
                        --exclude "*/tests/*"
                        --exclude "/usr/*"
        COMMAND ${GENHTML} coverage.info
                           --output-directory ${CMAKE_BINARY_DIR}/coverage_html
                           --title "gps-pipeline coverage"
                           --legend
        COMMAND ${CMAKE_COMMAND} -E echo
                "Coverage report: ${CMAKE_BINARY_DIR}/coverage_html/index.html"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS gps_pipeline_tests
        COMMENT "Generating lcov coverage report..."
    )
endif()
