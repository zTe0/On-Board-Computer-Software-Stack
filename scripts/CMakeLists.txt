cmake_minimum_required(VERSION 3.5)
project(obcsw VERSION 1.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------
# Toolchain settings
# ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE / clang-tidy integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
# Compiler warnings & hardening
# ---------------------------------------------------------------
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------
# Dependencies via FetchContent (zero local install required)
# ---------------------------------------------------------------
include(FetchContent)

# nlohmann/json — header-only JSON
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# yaml-cpp — YAML config loader
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# MinGW workaround: yaml-cpp 0.8.0 missing <cstdint> in emitterutils.cpp
if(MINGW)
    target_compile_options(yaml-cpp PRIVATE -include cstdint)
endif()

# Catch2 — unit test framework
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ---------------------------------------------------------------
# obcsw core static library
# ---------------------------------------------------------------
add_library(obcsw_core STATIC
    src/health/FaultDetector.cpp
    src/telemetry/TelemetryDispatcher.cpp
    src/serialization/TelemetryFrame.cpp
    src/serialization/TelemetrySerializer.cpp
    src/serialization/CommandDeserializer.cpp
    src/serialization/ConfigLoader.cpp
    src/command/CommandHandler.cpp
    src/command/CommandQueue.cpp
)

target_include_directories(obcsw_core 
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(obcsw_core PUBLIC
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
    $<IF:$<BOOL:${WIN32}>,ws2_32,pthread>
)

# ---------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------
add_executable(obcsw src/main.cpp)
target_link_libraries(obcsw PRIVATE obcsw_core)

# ---------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------
enable_testing()

add_executable(obcsw_tests tests/test_obcsw.cpp)
target_link_libraries(obcsw_tests PRIVATE obcsw_core Catch2::Catch2)

include(${Catch2_SOURCE_DIR}/contrib/Catch.cmake)
catch_discover_tests(obcsw_tests)

# ---------------------------------------------------------------
# Install rules (useful if cross-compiling for embedded target)
# ---------------------------------------------------------------
install(TARGETS obcsw DESTINATION bin)
install(FILES config/satellite.yaml DESTINATION etc/obcsw)
