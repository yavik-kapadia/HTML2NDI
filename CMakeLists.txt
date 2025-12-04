cmake_minimum_required(VERSION 3.21)
project(HTML2NDI VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# macOS deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Universal binary support (Intel + Apple Silicon)
option(BUILD_UNIVERSAL "Build universal binary for x86_64 and arm64" OFF)
if(BUILD_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# =============================================================================
# Dependencies
# =============================================================================

# CEF (Chromium Embedded Framework)
include(CEF)

# NDI SDK
include(NDI)

# cpp-httplib (header-only)
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# nlohmann/json for JSON handling
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Google Test for unit testing
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Prevent overriding parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# =============================================================================
# Source Files
# =============================================================================

set(APP_SOURCES
    src/app/main.cpp
    src/app/config.cpp
    src/app/application.cpp
)

set(CEF_SOURCES
    src/cef/cef_handler.cpp
    src/cef/cef_app.cpp
    src/cef/offscreen_renderer.cpp
)

set(NDI_SOURCES
    src/ndi/ndi_sender.cpp
    src/ndi/frame_pump.cpp
)

set(HTTP_SOURCES
    src/http/http_server.cpp
)

set(UTILS_SOURCES
    src/utils/logger.cpp
    src/utils/signal_handler.cpp
    src/utils/image_encode.cpp
)

# Helper sources
set(HELPER_SOURCES
    src/cef/cef_helper.cpp
)

# =============================================================================
# Target names and paths
# =============================================================================

set(MAIN_TARGET "html2ndi")
set(HELPER_TARGET "html2ndi_Helper")
set(HELPER_OUTPUT_NAME "html2ndi Helper")

# Output app bundle path
set(APP_BUNDLE "${CMAKE_BINARY_DIR}/bin/${MAIN_TARGET}.app")
set(APP_CONTENTS "${APP_BUNDLE}/Contents")
set(APP_MACOS "${APP_CONTENTS}/MacOS")
set(APP_FRAMEWORKS "${APP_CONTENTS}/Frameworks")
set(APP_RESOURCES "${APP_CONTENTS}/Resources")

# Helper app suffixes (CEF requirement for multi-process)
# Format: "name suffix:target suffix:plist suffix"
set(HELPER_APP_SUFFIXES
    "::"
    " (Alerts):_alerts:.alerts"
    " (GPU):_gpu:.gpu"
    " (Plugin):_plugin:.plugin"
    " (Renderer):_renderer:.renderer"
)

# =============================================================================
# Main Executable (macOS .app bundle)
# =============================================================================

add_executable(${MAIN_TARGET} MACOSX_BUNDLE
    ${APP_SOURCES}
    ${CEF_SOURCES}
    ${NDI_SOURCES}
    ${HTTP_SOURCES}
    ${UTILS_SOURCES}
)

target_include_directories(${MAIN_TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CEF_INCLUDE_DIR}
    ${NDI_INCLUDE_DIR}
)

target_link_libraries(${MAIN_TARGET} PRIVATE
    ${CEF_LIBRARIES}
    ${NDI_LIBRARIES}
    httplib::httplib
    nlohmann_json::nlohmann_json
)

# macOS frameworks
target_link_libraries(${MAIN_TARGET} PRIVATE
    "-framework Cocoa"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework CoreServices"
    "-framework IOKit"
    "-framework Security"
)

# Bundle properties
set_target_properties(${MAIN_TARGET} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "HTML2NDI"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.1"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.html2ndi.app"
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
    MACOSX_RPATH TRUE
)

# =============================================================================
# CEF Helper App Bundles (required for CEF multi-process architecture)
# =============================================================================

foreach(_suffix_item ${HELPER_APP_SUFFIXES})
    # Parse the suffix string (name:target:plist)
    string(REPLACE ":" ";" _suffix_list "${_suffix_item}")
    list(GET _suffix_list 0 _name_suffix)
    list(GET _suffix_list 1 _target_suffix)
    list(GET _suffix_list 2 _plist_suffix)
    
    # Define helper target and output names
    set(_helper_target "${HELPER_TARGET}${_target_suffix}")
    set(_helper_output_name "${HELPER_OUTPUT_NAME}${_name_suffix}")
    
    # Generate helper-specific Info.plist
    set(_helper_plist "${CMAKE_BINARY_DIR}/helper-Info${_target_suffix}.plist")
    file(READ "${CMAKE_SOURCE_DIR}/resources/helper-Info.plist" _plist_contents)
    string(REPLACE "\${HELPER_NAME}" "${_helper_output_name}" _plist_contents "${_plist_contents}")
    string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents "${_plist_contents}")
    file(WRITE "${_helper_plist}" "${_plist_contents}")
    
    # Create helper executable as app bundle
    add_executable(${_helper_target} MACOSX_BUNDLE ${HELPER_SOURCES})
    
    target_include_directories(${_helper_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CEF_INCLUDE_DIR}
    )
    
    target_link_libraries(${_helper_target} PRIVATE
        ${CEF_LIBRARIES}
    )
    
    set_target_properties(${_helper_target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${_helper_plist}"
        OUTPUT_NAME "${_helper_output_name}"
        # RPATH to find CEF framework from helper's MacOS directory
        # Path: html2ndi Helper.app/Contents/MacOS -> up 3 levels -> main Frameworks dir
        INSTALL_RPATH "@executable_path/../../../;@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
        MACOSX_RPATH TRUE
    )
    
    # Add helper as dependency of main target
    add_dependencies(${MAIN_TARGET} ${_helper_target})
    
    # Copy helper app bundle to main app's Frameworks directory
    # Use quotes to handle spaces and parentheses in names
    add_custom_command(TARGET ${MAIN_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/bin/${_helper_output_name}.app"
            "${APP_FRAMEWORKS}/${_helper_output_name}.app"
        # Create Frameworks symlink inside helper app pointing to main app's Frameworks
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${APP_FRAMEWORKS}/${_helper_output_name}.app/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "../../../Chromium Embedded Framework.framework"
            "${APP_FRAMEWORKS}/${_helper_output_name}.app/Contents/Frameworks/Chromium Embedded Framework.framework"
        COMMENT "Copying and linking helper app: ${_helper_output_name}.app"
        VERBATIM
    )
endforeach()

# =============================================================================
# Post-build: Setup .app bundle structure
# =============================================================================

# Copy CEF Framework to bundle
add_custom_command(TARGET ${MAIN_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_FRAMEWORKS}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_FRAMEWORK_DIR}"
        "${APP_FRAMEWORKS}/Chromium Embedded Framework.framework"
    COMMENT "Copying CEF framework to bundle..."
)

# Copy CEF Resources to bundle
add_custom_command(TARGET ${MAIN_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_RESOURCES}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_RESOURCES_DIR}"
        "${APP_RESOURCES}"
    COMMENT "Copying CEF resources to bundle..."
)

# Copy NDI library to Frameworks
add_custom_command(TARGET ${MAIN_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${NDI_LIBRARY}"
        "${APP_FRAMEWORKS}/libndi.dylib"
    COMMENT "Copying NDI library to bundle..."
)

# =============================================================================
# Installation
# =============================================================================

install(TARGETS ${MAIN_TARGET}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CEF_FRAMEWORK_DIR}
    DESTINATION Frameworks
    USE_SOURCE_PERMISSIONS
)

install(FILES ${NDI_LIBRARY}
    DESTINATION lib
)

# =============================================================================
# Unit Tests
# =============================================================================

if(BUILD_TESTS)
    # Unit test sources (tests that don't require CEF/NDI runtime)
    set(TEST_SOURCES
        tests/cpp/test_main.cpp
        tests/cpp/test_config.cpp
        tests/cpp/test_logger.cpp
        tests/cpp/test_http_api.cpp
    )
    
    # Sources that can be tested without CEF/NDI runtime
    set(TESTABLE_SOURCES
        src/app/config.cpp
        src/utils/logger.cpp
    )
    
    # Create test executable
    add_executable(html2ndi_tests ${TEST_SOURCES} ${TESTABLE_SOURCES})
    
    target_include_directories(html2ndi_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CEF_INCLUDE_DIR}
        ${NDI_INCLUDE_DIR}
    )
    
    target_link_libraries(html2ndi_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        httplib::httplib
        nlohmann_json::nlohmann_json
        "-framework CoreFoundation"
        "-framework Security"
    )
    
    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(html2ndi_tests)
    
    # Add custom run_tests target (can't use "test" - reserved by CTest)
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS html2ndi_tests
        COMMENT "Running unit tests..."
    )
endif()

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== HTML2NDI Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Target architectures: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "CEF: ${CEF_VERSION}")
message(STATUS "NDI: ${NDI_VERSION}")
message(STATUS "Tests: ${BUILD_TESTS}")
message(STATUS "")
