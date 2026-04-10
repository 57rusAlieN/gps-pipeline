cmake_minimum_required(VERSION 3.14)

project(gps_pipeline CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
