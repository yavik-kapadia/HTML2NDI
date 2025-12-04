cmake_minimum_required(VERSION 3.21)
project(HTML2NDI VERSION 1.0.0 LANGUAGES CXX)

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
option(BUILD_UNIVERSAL "Build universal binary for x86_64 and arm64" ON)
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
)

# =============================================================================
# Main Executable (macOS .app bundle)
# =============================================================================

add_executable(html2ndi MACOSX_BUNDLE
    ${APP_SOURCES}
    ${CEF_SOURCES}
    ${NDI_SOURCES}
    ${HTTP_SOURCES}
    ${UTILS_SOURCES}
)

target_include_directories(html2ndi PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CEF_INCLUDE_DIR}
    ${NDI_INCLUDE_DIR}
)

target_link_libraries(html2ndi PRIVATE
    ${CEF_LIBRARIES}
    ${NDI_LIBRARIES}
    httplib::httplib
    nlohmann_json::nlohmann_json
)

# macOS frameworks
target_link_libraries(html2ndi PRIVATE
    "-framework Cocoa"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework CoreServices"
    "-framework IOKit"
    "-framework Security"
)

# Bundle properties
set_target_properties(html2ndi PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "HTML2NDI"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.html2ndi.app"
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
    MACOSX_RPATH TRUE
)

# =============================================================================
# CEF Helper Executable (required for CEF subprocess)
# =============================================================================

add_executable(html2ndi_helper
    src/cef/cef_helper.cpp
)

target_include_directories(html2ndi_helper PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CEF_INCLUDE_DIR}
)

target_link_libraries(html2ndi_helper PRIVATE
    ${CEF_LIBRARIES}
)

set_target_properties(html2ndi_helper PROPERTIES
    INSTALL_RPATH "@executable_path;@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
    MACOSX_RPATH TRUE
)

# =============================================================================
# Installation
# =============================================================================

install(TARGETS html2ndi
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

install(TARGETS html2ndi_helper
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
# Post-build: Setup .app bundle structure
# =============================================================================

# Get bundle paths
set(BUNDLE_PATH "$<TARGET_BUNDLE_DIR:html2ndi>")
set(BUNDLE_CONTENTS "${BUNDLE_PATH}/Contents")
set(BUNDLE_MACOS "${BUNDLE_CONTENTS}/MacOS")
set(BUNDLE_FRAMEWORKS "${BUNDLE_CONTENTS}/Frameworks")
set(BUNDLE_RESOURCES "${BUNDLE_CONTENTS}/Resources")

# Copy CEF Framework to bundle
add_custom_command(TARGET html2ndi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_FRAMEWORKS}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_FRAMEWORK_DIR}"
        "${BUNDLE_FRAMEWORKS}/Chromium Embedded Framework.framework"
    COMMENT "Copying CEF framework to bundle..."
)

# Copy CEF Resources to bundle
add_custom_command(TARGET html2ndi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_RESOURCES_DIR}"
        "${BUNDLE_RESOURCES}"
    COMMENT "Copying CEF resources to bundle..."
)

# Copy NDI library to Frameworks
add_custom_command(TARGET html2ndi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${NDI_LIBRARY}"
        "${BUNDLE_FRAMEWORKS}/libndi.dylib"
    COMMENT "Copying NDI library to bundle..."
)

# Copy helper to MacOS directory
add_custom_command(TARGET html2ndi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:html2ndi_helper>"
        "${BUNDLE_MACOS}/html2ndi_helper"
    COMMENT "Copying helper to bundle..."
)

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
message(STATUS "")

