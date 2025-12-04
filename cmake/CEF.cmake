# CEF.cmake - Download and configure Chromium Embedded Framework
#
# This module will:
# 1. Download CEF binary distribution for macOS
# 2. Set up include directories
# 3. Configure libraries for linking

set(CEF_VERSION "120.1.10+g3ce3184+chromium-120.0.6099.129" CACHE STRING "CEF version")
set(CEF_PLATFORM "macos64" CACHE STRING "CEF platform")

# Determine architecture-specific CEF build
if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(CEF_PLATFORM "macosarm64")
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    set(CEF_PLATFORM "macos64")
else()
    # Default based on host architecture
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(HOST_ARCH STREQUAL "arm64")
        set(CEF_PLATFORM "macosarm64")
    else()
        set(CEF_PLATFORM "macos64")
    endif()
endif()

set(CEF_DISTRIBUTION "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}")
set(CEF_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/cef")
set(CEF_ROOT "${CEF_DOWNLOAD_DIR}/${CEF_DISTRIBUTION}")

# Check if CEF is already downloaded
if(NOT EXISTS "${CEF_ROOT}")
    message(STATUS "Downloading CEF ${CEF_VERSION} for ${CEF_PLATFORM}...")
    
    # CEF download URL
    string(REPLACE "+" "%2B" CEF_VERSION_URL "${CEF_VERSION}")
    set(CEF_URL "https://cef-builds.spotifycdn.com/${CEF_DISTRIBUTION}.tar.bz2")
    
    set(CEF_ARCHIVE "${CEF_DOWNLOAD_DIR}/${CEF_DISTRIBUTION}.tar.bz2")
    
    file(MAKE_DIRECTORY ${CEF_DOWNLOAD_DIR})
    
    if(NOT EXISTS "${CEF_ARCHIVE}")
        message(STATUS "Downloading from ${CEF_URL}")
        file(DOWNLOAD
            "${CEF_URL}"
            "${CEF_ARCHIVE}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )
        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR)
        if(DOWNLOAD_ERROR)
            message(FATAL_ERROR "Failed to download CEF: ${DOWNLOAD_STATUS}")
        endif()
    endif()
    
    message(STATUS "Extracting CEF...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xjf "${CEF_ARCHIVE}"
        WORKING_DIRECTORY "${CEF_DOWNLOAD_DIR}"
        RESULT_VARIABLE EXTRACT_RESULT
    )
    if(NOT EXTRACT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to extract CEF archive")
    endif()
endif()

# Set CEF paths
set(CEF_INCLUDE_DIR "${CEF_ROOT}" "${CEF_ROOT}/include")
set(CEF_RESOURCES_DIR "${CEF_ROOT}/Release/Chromium Embedded Framework.framework/Resources")
set(CEF_FRAMEWORK_DIR "${CEF_ROOT}/Release/Chromium Embedded Framework.framework")

# Build libcef_dll_wrapper if not already built
set(CEF_WRAPPER_LIB "${CEF_ROOT}/build/libcef_dll_wrapper/libcef_dll_wrapper.a")

if(NOT EXISTS "${CEF_WRAPPER_LIB}")
    message(STATUS "Building CEF DLL wrapper...")
    
    file(MAKE_DIRECTORY "${CEF_ROOT}/build")
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} 
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
            -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
            ..
        WORKING_DIRECTORY "${CEF_ROOT}/build"
        RESULT_VARIABLE CMAKE_RESULT
    )
    if(NOT CMAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to configure CEF wrapper")
    endif()
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --target libcef_dll_wrapper --config Release
        WORKING_DIRECTORY "${CEF_ROOT}/build"
        RESULT_VARIABLE BUILD_RESULT
    )
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build CEF wrapper")
    endif()
endif()

# Set libraries
set(CEF_LIBRARIES
    "${CEF_WRAPPER_LIB}"
    "${CEF_FRAMEWORK_DIR}/Chromium Embedded Framework"
)

# Verify CEF was found
if(EXISTS "${CEF_ROOT}")
    message(STATUS "CEF found: ${CEF_ROOT}")
    message(STATUS "CEF platform: ${CEF_PLATFORM}")
else()
    message(FATAL_ERROR "CEF not found at ${CEF_ROOT}")
endif()

