cmake_minimum_required(VERSION 3.14)

project(LiveTuner
    VERSION 1.0.0
    DESCRIPTION "Live parameter tuning library for C++"
    LANGUAGES CXX
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(LIVETUNER_BUILD_EXAMPLES "Build example programs" OFF)
option(LIVETUNER_BUILD_TESTS "Build test programs" OFF)
option(LIVETUNER_INSTALL "Generate install target" ON)

# Find required packages
find_package(Threads REQUIRED)

# Header-only library target
add_library(LiveTuner_header_only INTERFACE)
add_library(LiveTuner::header_only ALIAS LiveTuner_header_only)

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

# Examples
if(LIVETUNER_BUILD_EXAMPLES)
    add_executable(livetuner_example examples/example.cpp)
    target_link_libraries(livetuner_example PRIVATE LiveTuner::header_only)
    
    # nlohmann/json example (only if nlohmann_json is available)
    find_package(nlohmann_json QUIET)
    if(nlohmann_json_FOUND)
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
    
    add_executable(livetuner_test test_compile.cpp)
    target_link_libraries(livetuner_test PRIVATE LiveTuner::header_only)
    
    add_test(NAME livetuner_compile_test COMMAND livetuner_test)
endif()

# Installation
if(LIVETUNER_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(
        FILES 
            include/LiveTuner.h
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
