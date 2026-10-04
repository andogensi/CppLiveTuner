cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(LiveTuner
    VERSION 1.0.0
    DESCRIPTION "Live parameter tuning library for C++"
    LANGUAGES CXX
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Options
option(LIVETUNER_BUILD_EXAMPLES "Build example programs" OFF)
option(LIVETUNER_BUILD_TESTS "Build test programs" OFF)
option(LIVETUNER_INSTALL "Generate install target" ON)
option(LIVETUNER_TEST_OPTIONAL_BACKENDS "Test pinned external picojson and nlohmann/json" OFF)
option(LIVETUNER_BUILD_FUZZER "Build the Clang libFuzzer parser target" OFF)
set(LIVETUNER_SANITIZER "" CACHE STRING "Test sanitizer: address or thread")
set_property(CACHE LIVETUNER_SANITIZER PROPERTY STRINGS "" address thread)
if(LIVETUNER_SANITIZER AND NOT LIVETUNER_SANITIZER MATCHES "^(address|thread)$")
    message(FATAL_ERROR "Unsupported LIVETUNER_SANITIZER")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Header-only library target
add_library(LiveTuner_header_only INTERFACE)
add_library(LiveTuner::header_only ALIAS LiveTuner_header_only)
set_target_properties(LiveTuner_header_only PROPERTIES EXPORT_NAME header_only)

target_include_directories(LiveTuner_header_only
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(LiveTuner_header_only INTERFACE cxx_std_17)
target_link_libraries(LiveTuner_header_only INTERFACE Threads::Threads)

# Platform-specific libraries
if(WIN32)
    # Windows: No additional libraries needed (uses Win32 API)
elseif(APPLE)
    # macOS: Link CoreServices for FSEvents
    target_link_libraries(LiveTuner_header_only INTERFACE "-framework CoreServices")
endif()

# Default alias
add_library(LiveTuner::LiveTuner ALIAS LiveTuner_header_only)

if(LIVETUNER_TEST_OPTIONAL_BACKENDS)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
        URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187)
    FetchContent_Declare(picojson
        URL https://codeload.github.com/kazuho/picojson/tar.gz/111c9be5188f7350c2eac9ddaedd8cca3d7bf394
        URL_HASH SHA256=671f89832a17e9e71398f80c0a326afa2ebe81f4c26d5a9992e1fcd0888ae151)
    FetchContent_MakeAvailable(nlohmann_json picojson)
endif()

# Examples
if(LIVETUNER_BUILD_EXAMPLES)
    add_executable(livetuner_example examples/example.cpp)
    target_link_libraries(livetuner_example PRIVATE LiveTuner::header_only)
    
    # nlohmann/json example (only if nlohmann_json is available)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        find_package(nlohmann_json QUIET)
    endif()
    if(TARGET nlohmann_json::nlohmann_json)
        add_executable(livetuner_example_nlohmann examples/example_nlohmann.cpp)
        target_link_libraries(livetuner_example_nlohmann PRIVATE LiveTuner::header_only nlohmann_json::nlohmann_json)
        target_include_directories(livetuner_example_nlohmann PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    else()
        message(STATUS "nlohmann_json not found, skipping livetuner_example_nlohmann")
    endif()
    
    # Copy example config files
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/params.txt
        ${CMAKE_CURRENT_BINARY_DIR}/params.txt
        COPYONLY
    )
endif()

# Tests
if(LIVETUNER_BUILD_TESTS)
    enable_testing()
    function(livetuner_check_target target)
        target_link_libraries(${target} PRIVATE LiveTuner::header_only)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
        endif()
        if(LIVETUNER_SANITIZER)
            if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                message(FATAL_ERROR "Sanitizer configuration requires GCC or Clang on a Unix-like host")
            endif()
            if(LIVETUNER_SANITIZER STREQUAL "address")
                set(sanitizers address,undefined)
            else()
                set(sanitizers thread)
            endif()
            target_compile_options(${target} PRIVATE -fsanitize=${sanitizers} -fno-omit-frame-pointer -g)
            target_link_options(${target} PRIVATE -fsanitize=${sanitizers})
        endif()
    endfunction()
    add_executable(livetuner_test test_compile.cpp)
    livetuner_check_target(livetuner_test)
    add_test(NAME livetuner_compile_test COMMAND livetuner_test)

    set(test_backends embedded int64)
    if(LIVETUNER_TEST_OPTIONAL_BACKENDS)
        list(APPEND test_backends external nlohmann)
    endif()
    foreach(backend IN LISTS test_backends)
        set(target livetuner_security_${backend})
        add_executable(${target} tests/security_tests.cpp)
        livetuner_check_target(${target})
        if(backend STREQUAL "external")
            target_compile_definitions(${target} PRIVATE LIVETUNER_USE_EXTERNAL_PICOJSON)
            target_include_directories(${target} SYSTEM PRIVATE ${picojson_SOURCE_DIR})
        elseif(backend STREQUAL "nlohmann")
            target_compile_definitions(${target} PRIVATE LIVETUNER_USE_NLOHMANN_JSON)
            target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json)
        elseif(backend STREQUAL "int64")
            target_compile_definitions(${target} PRIVATE PICOJSON_USE_INT64)
        endif()
        foreach(test parser ingress lifetime watcher concurrency)
            set(workdir ${CMAKE_CURRENT_BINARY_DIR}/test-work/${backend}/${test})
            file(MAKE_DIRECTORY ${workdir})
            add_test(NAME ${backend}_${test} COMMAND ${target} ${test})
            set_tests_properties(${backend}_${test} PROPERTIES WORKING_DIRECTORY ${workdir} TIMEOUT 60)
        endforeach()
    endforeach()

    add_executable(livetuner_multi_tu tests/multi_tu_impl.cpp tests/multi_tu_main.cpp)
    livetuner_check_target(livetuner_multi_tu)
    add_test(NAME livetuner_multi_tu COMMAND livetuner_multi_tu)
    add_executable(livetuner_disabled tests/disabled.cpp)
    livetuner_check_target(livetuner_disabled)
    add_test(NAME livetuner_disabled COMMAND livetuner_disabled)
    set_tests_properties(livetuner_compile_test livetuner_multi_tu livetuner_disabled PROPERTIES TIMEOUT 30)
    if(LIVETUNER_TEST_OPTIONAL_BACKENDS)
        add_executable(livetuner_disabled_nlohmann tests/disabled.cpp)
        livetuner_check_target(livetuner_disabled_nlohmann)
        target_compile_definitions(livetuner_disabled_nlohmann PRIVATE LIVETUNER_USE_NLOHMANN_JSON)
        target_link_libraries(livetuner_disabled_nlohmann PRIVATE nlohmann_json::nlohmann_json)
        add_test(NAME livetuner_disabled_nlohmann COMMAND livetuner_disabled_nlohmann)
        set_tests_properties(livetuner_disabled_nlohmann PROPERTIES TIMEOUT 30)
    endif()
endif()

if(LIVETUNER_BUILD_FUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC)
        message(FATAL_ERROR "libFuzzer requires Clang with the Unix command-line interface")
    endif()
    add_executable(livetuner_fuzz tests/fuzz_parser.cpp)
    target_link_libraries(livetuner_fuzz PRIVATE LiveTuner::header_only)
    target_compile_options(livetuner_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
    target_link_options(livetuner_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

# Installation
if(LIVETUNER_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(
        FILES 
            include/LiveTuner.h
            include/LiveTunerTest.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install targets
    install(
        TARGETS LiveTuner_header_only
        EXPORT LiveTunerTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install export
    install(
        EXPORT LiveTunerTargets
        FILE LiveTunerTargets.cmake
        NAMESPACE LiveTuner::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LiveTuner
    )

    # Generate package config
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LiveTunerConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LiveTunerConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LiveTuner
    )

    # Generate version config
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/LiveTunerConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/LiveTunerConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/LiveTunerConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LiveTuner
    )

    # Generate and install pkg-config file
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LiveTuner.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/LiveTuner.pc
        @ONLY
    )

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/LiveTuner.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "LiveTuner Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build examples: ${LIVETUNER_BUILD_EXAMPLES}")
message(STATUS "  Build tests:    ${LIVETUNER_BUILD_TESTS}")
message(STATUS "  Install:        ${LIVETUNER_INSTALL}")
message(STATUS "")
