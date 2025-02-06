cmake_minimum_required(VERSION 3.28...3.30)

project(libdshowcapture)

option(BUILD_SHARED_LIBS "Build shared library" ON)

# Set C and C++ language standards to C17 and C++17
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Disable C and C++ extensions
set(CMAKE_C_EXTENSIONS FALSE)
set(CMAKE_CXX_EXTENSIONS FALSE)

# Set symbols to be hidden by default for C and C++
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

set(
  libdshowcapture_SOURCES
  external/capture-device-support/Library/EGAVResult.cpp
  external/capture-device-support/Library/ElgatoUVCDevice.cpp
  external/capture-device-support/Library/win/EGAVHIDImplementation.cpp
  external/capture-device-support/SampleCode/DriverInterface.cpp
  source/capture-filter.cpp
  source/output-filter.cpp
  source/dshowcapture.cpp
  source/dshowencode.cpp
  source/device.cpp
  source/device-vendor.cpp
  source/encoder.cpp
  source/dshow-base.cpp
  source/dshow-demux.cpp
  source/dshow-enum.cpp
  source/dshow-formats.cpp
  source/dshow-media-type.cpp
  source/dshow-encoded-device.cpp
  source/log.cpp
)

set(
  libdshowcapture_HEADERS
  dshowcapture.hpp
  source/external/IVideoCaptureFilter.h
  source/capture-filter.hpp
  source/output-filter.hpp
  source/device.hpp
  source/encoder.hpp
  source/dshow-base.hpp
  source/dshow-demux.hpp
  source/dshow-device-defs.hpp
  source/dshow-enum.hpp
  source/dshow-formats.hpp
  source/dshow-media-type.hpp
  source/log.hpp
)

add_library(libdshowcapture ${libdshowcapture_SOURCES} ${libdshowcapture_HEADERS})

# Set /W4
# Disable pointless constant condition warnings
target_compile_options(libdshowcapture PRIVATE /W4 /wd4127 /wd4201)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  target_link_options(libdshowcapture PRIVATE /SAFESEH:NO)
endif()

target_include_directories(libdshowcapture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/capture-device-support/Library)

target_compile_definitions(
  libdshowcapture
  PRIVATE
    UNICODE
    _UNICODE
    $<$<CONFIG:DEBUG>:DEBUG>
    $<$<CONFIG:DEBUG>:_DEBUG>
    $<$<BOOL:${BUILD_SHARED_LIBS}>:DSHOWCAPTURE_EXPORTS>
    _UP_WINDOWS=1
)

target_link_libraries(
  libdshowcapture
  PRIVATE setupapi strmiids ksuser winmm wmcodecdspuuid
)
