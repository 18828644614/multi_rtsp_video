cmake_minimum_required(VERSION 3.20)

project(multi_rtsp_video_analysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(FindPackageHandleStandardArgs)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(app_config STATIC
    src/config/config.cpp
)

target_include_directories(app_config
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(app_config
    PUBLIC
        yaml-cpp::yaml-cpp
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(app_config PUBLIC stdc++fs)
endif()

enable_project_warnings(app_config)

set(FFMPEG_ROOT "" CACHE PATH "FFmpeg installation prefix")
if(NOT FFMPEG_ROOT AND DEFINED ENV{FFMPEG_ROOT})
    set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG_PC_AVCODEC QUIET libavcodec)
    pkg_check_modules(FFMPEG_PC_AVFORMAT QUIET libavformat)
    pkg_check_modules(FFMPEG_PC_AVUTIL QUIET libavutil)
    pkg_check_modules(FFMPEG_PC_SWSCALE QUIET libswscale)
endif()

set(_ffmpeg_include_hints)
set(_ffmpeg_library_hints)
if(FFMPEG_ROOT)
    list(APPEND _ffmpeg_include_hints "${FFMPEG_ROOT}/include")
    list(APPEND _ffmpeg_library_hints "${FFMPEG_ROOT}/lib")
endif()
list(APPEND _ffmpeg_include_hints
    ${FFMPEG_PC_AVCODEC_INCLUDE_DIRS}
    ${FFMPEG_PC_AVFORMAT_INCLUDE_DIRS}
    ${FFMPEG_PC_AVUTIL_INCLUDE_DIRS}
    ${FFMPEG_PC_SWSCALE_INCLUDE_DIRS}
)
list(APPEND _ffmpeg_library_hints
    ${FFMPEG_PC_AVCODEC_LIBRARY_DIRS}
    ${FFMPEG_PC_AVFORMAT_LIBRARY_DIRS}
    ${FFMPEG_PC_AVUTIL_LIBRARY_DIRS}
    ${FFMPEG_PC_SWSCALE_LIBRARY_DIRS}
)

find_path(FFMPEG_INCLUDE_DIR
    NAMES libavformat/avformat.h
    HINTS ${_ffmpeg_include_hints}
    PATH_SUFFIXES include
)
find_library(FFMPEG_AVCODEC_LIBRARY
    NAMES avcodec libavcodec
    HINTS ${_ffmpeg_library_hints}
)
find_library(FFMPEG_AVFORMAT_LIBRARY
    NAMES avformat libavformat
    HINTS ${_ffmpeg_library_hints}
)
find_library(FFMPEG_AVUTIL_LIBRARY
    NAMES avutil libavutil
    HINTS ${_ffmpeg_library_hints}
)
find_library(FFMPEG_SWSCALE_LIBRARY
    NAMES swscale libswscale
    HINTS ${_ffmpeg_library_hints}
)

find_package_handle_standard_args(FFMPEG
    REQUIRED_VARS
        FFMPEG_INCLUDE_DIR
        FFMPEG_AVCODEC_LIBRARY
        FFMPEG_AVFORMAT_LIBRARY
        FFMPEG_AVUTIL_LIBRARY
        FFMPEG_SWSCALE_LIBRARY
)
if(NOT FFMPEG_FOUND)
    message(FATAL_ERROR
        "FFmpeg development files were not found. Set FFMPEG_ROOT to an FFmpeg prefix containing include/ and lib/."
    )
endif()

add_library(ffmpeg_decoder STATIC
    src/media/ffmpeg_mp4_decoder.cpp
)

target_include_directories(ffmpeg_decoder
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${FFMPEG_INCLUDE_DIR}
)

target_link_libraries(ffmpeg_decoder
    PUBLIC
        ${FFMPEG_AVFORMAT_LIBRARY}
        ${FFMPEG_AVCODEC_LIBRARY}
        ${FFMPEG_SWSCALE_LIBRARY}
        ${FFMPEG_AVUTIL_LIBRARY}
)

enable_project_warnings(ffmpeg_decoder)

add_executable(multi_rtsp_video_analysis
    src/main.cpp
)

target_include_directories(multi_rtsp_video_analysis
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(multi_rtsp_video_analysis
    PRIVATE
        app_config
        ffmpeg_decoder
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(multi_rtsp_video_analysis PRIVATE stdc++fs)
endif()

enable_project_warnings(multi_rtsp_video_analysis)

enable_testing()

add_executable(smoke_test
    tests/smoke_test.cpp
)

target_include_directories(smoke_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

enable_project_warnings(smoke_test)

add_test(NAME smoke_test COMMAND smoke_test)

add_executable(config_test
    tests/config_test.cpp
)

target_include_directories(config_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(config_test
    PRIVATE
        app_config
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(config_test PRIVATE stdc++fs)
endif()

enable_project_warnings(config_test)

add_test(NAME config_test COMMAND config_test)

add_executable(mp4_decoder_test
    tests/mp4_decoder_test.cpp
)

target_include_directories(mp4_decoder_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mp4_decoder_test
    PRIVATE
        ffmpeg_decoder
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(mp4_decoder_test PRIVATE stdc++fs)
endif()

enable_project_warnings(mp4_decoder_test)

add_test(
    NAME mp4_decoder_test
    COMMAND mp4_decoder_test "${PROJECT_SOURCE_DIR}/data/demo.mp4"
)
