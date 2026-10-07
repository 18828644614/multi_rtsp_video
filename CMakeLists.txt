cmake_minimum_required(VERSION 3.20)

project(multi_rtsp_video_analysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(FindPackageHandleStandardArgs)
find_package(Threads REQUIRED)
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc)

set(ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime C++ installation prefix")
if(NOT ONNXRUNTIME_ROOT AND DEFINED ENV{ONNXRUNTIME_ROOT})
    set(ONNXRUNTIME_ROOT "$ENV{ONNXRUNTIME_ROOT}" CACHE PATH "ONNX Runtime C++ installation prefix" FORCE)
endif()
if(NOT ONNXRUNTIME_ROOT AND WIN32)
    set(_local_onnxruntime_root "D:/Onnx/onnxruntime-win-x64-1.30.0")
    if(EXISTS "${_local_onnxruntime_root}/include/onnxruntime_cxx_api.h")
        set(ONNXRUNTIME_ROOT "${_local_onnxruntime_root}" CACHE PATH "ONNX Runtime C++ installation prefix" FORCE)
    endif()
endif()

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    HINTS "${ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
)
find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    HINTS "${ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
)

set(_onnxruntime_required_vars
    ONNXRUNTIME_INCLUDE_DIR
    ONNXRUNTIME_LIBRARY
)
if(WIN32)
    find_file(ONNXRUNTIME_RUNTIME_LIBRARY
        NAMES onnxruntime.dll
        HINTS "${ONNXRUNTIME_ROOT}"
        PATH_SUFFIXES lib bin
        NO_DEFAULT_PATH
    )
    list(APPEND _onnxruntime_required_vars ONNXRUNTIME_RUNTIME_LIBRARY)
endif()

find_package_handle_standard_args(ONNXRUNTIME
    REQUIRED_VARS ${_onnxruntime_required_vars}
)
if(NOT ONNXRUNTIME_FOUND)
    message(FATAL_ERROR
        "ONNX Runtime C++ was not found. Set ONNXRUNTIME_ROOT to the extracted CPU package "
        "containing include/onnxruntime_cxx_api.h and lib/onnxruntime."
    )
endif()

if(WIN32)
    add_library(onnxruntime::onnxruntime SHARED IMPORTED GLOBAL)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_IMPLIB "${ONNXRUNTIME_LIBRARY}"
        IMPORTED_LOCATION "${ONNXRUNTIME_RUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )
else()
    add_library(onnxruntime::onnxruntime UNKNOWN IMPORTED GLOBAL)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )
endif()

if(MINGW)
    add_compile_options(-mthreads)
    add_link_options(-mthreads)
endif()

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

if(FFMPEG_ROOT AND NOT EXISTS "${FFMPEG_ROOT}")
    message(WARNING "FFMPEG_ROOT does not exist and will be ignored: ${FFMPEG_ROOT}")
    unset(FFMPEG_ROOT CACHE)
    set(FFMPEG_ROOT "")
endif()

if(NOT FFMPEG_ROOT)
    find_program(FFMPEG_EXECUTABLE NAMES ffmpeg)
    if(FFMPEG_EXECUTABLE)
        get_filename_component(_ffmpeg_bin_dir "${FFMPEG_EXECUTABLE}" DIRECTORY)
        get_filename_component(_ffmpeg_auto_root "${_ffmpeg_bin_dir}" DIRECTORY)
        if(EXISTS "${_ffmpeg_auto_root}/include" OR EXISTS "${_ffmpeg_auto_root}/lib")
            set(FFMPEG_ROOT "${_ffmpeg_auto_root}" CACHE PATH "FFmpeg installation prefix" FORCE)
            message(STATUS "Auto-detected FFmpeg root: ${FFMPEG_ROOT}")
        endif()
    endif()
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
        "FFmpeg development libraries were not found under '${FFMPEG_ROOT}/lib'. "
        "MSVC requires avcodec.lib, avformat.lib, avutil.lib and swscale.lib; "
        "the ffmpeg.exe runtime alone is not sufficient."
    )
endif()

add_library(vision_manifest STATIC
    src/vision/model_manifest.cpp
)

target_include_directories(vision_manifest
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(vision_manifest
    PUBLIC
        yaml-cpp::yaml-cpp
)

enable_project_warnings(vision_manifest)

add_library(vision_detector STATIC
    src/vision/fake_detector.cpp
)

target_include_directories(vision_detector
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

enable_project_warnings(vision_detector)

add_library(vision_onnx_detector STATIC
    src/vision/onnx_detector.cpp
    src/vision/yolo26.cpp
)

target_include_directories(vision_onnx_detector
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(vision_onnx_detector
    PUBLIC
        vision_detector
    PRIVATE
        vision_manifest
        ${OpenCV_LIBS}
        onnxruntime::onnxruntime
)

enable_project_warnings(vision_onnx_detector)

add_library(detection_jsonl_sink STATIC
    src/output/detection_jsonl_sink.cpp
)

target_include_directories(detection_jsonl_sink
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

enable_project_warnings(detection_jsonl_sink)

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


add_library(frame_pipeline STATIC
    src/pipeline/frame_queue.cpp
    src/pipeline/frame_pipeline.cpp
)

target_include_directories(frame_pipeline
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(frame_pipeline
    PUBLIC
        Threads::Threads
)

enable_project_warnings(frame_pipeline)
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
        detection_jsonl_sink
        ffmpeg_decoder
        frame_pipeline
        vision_onnx_detector
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

add_executable(detection_test
    tests/detection_test.cpp
)

target_include_directories(detection_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

enable_project_warnings(detection_test)

add_test(NAME detection_test COMMAND detection_test)

add_executable(detection_jsonl_test
    tests/detection_jsonl_test.cpp
)

target_include_directories(detection_jsonl_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(detection_jsonl_test
    PRIVATE
        detection_jsonl_sink
        vision_detector
        yaml-cpp::yaml-cpp
)

enable_project_warnings(detection_jsonl_test)

add_test(NAME detection_jsonl_test COMMAND detection_jsonl_test)

add_executable(yolo26_postprocessor_test
    tests/yolo26_postprocessor_test.cpp
)

target_include_directories(yolo26_postprocessor_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(yolo26_postprocessor_test
    PRIVATE
        vision_onnx_detector
)

enable_project_warnings(yolo26_postprocessor_test)

add_test(NAME yolo26_postprocessor_test COMMAND yolo26_postprocessor_test)

add_executable(onnx_detector_jsonl_test
    tests/onnx_detector_jsonl_test.cpp
)

target_include_directories(onnx_detector_jsonl_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(onnx_detector_jsonl_test
    PRIVATE
        detection_jsonl_sink
        vision_onnx_detector
        yaml-cpp::yaml-cpp
)

enable_project_warnings(onnx_detector_jsonl_test)

if(EXISTS "${PROJECT_SOURCE_DIR}/models/detector.onnx")
    add_test(
        NAME onnx_detector_jsonl_test
        COMMAND onnx_detector_jsonl_test
            "${PROJECT_SOURCE_DIR}/models/detector.onnx"
            "${PROJECT_SOURCE_DIR}/models/detector-manifest.yaml"
    )
else()
    message(STATUS "Skipping onnx_detector_jsonl_test because models/detector.onnx is absent")
endif()

function(copy_detector_runtime_dependencies target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_RUNTIME_LIBRARY}"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM
        )
        foreach(opencv_target IN LISTS OpenCV_LIBS)
            if(TARGET ${opencv_target})
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${opencv_target}>"
                        "$<TARGET_FILE_DIR:${target}>"
                    VERBATIM
                )
            endif()
        endforeach()
    endif()
endfunction()

copy_detector_runtime_dependencies(multi_rtsp_video_analysis)
copy_detector_runtime_dependencies(yolo26_postprocessor_test)
copy_detector_runtime_dependencies(onnx_detector_jsonl_test)

add_executable(model_manifest_test
    tests/model_manifest_test.cpp
)

target_include_directories(model_manifest_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(model_manifest_test
    PRIVATE
        vision_manifest
)

enable_project_warnings(model_manifest_test)

add_test(NAME model_manifest_test COMMAND model_manifest_test)

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


add_executable(frame_queue_test
    tests/frame_queue_test.cpp
)

target_include_directories(frame_queue_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(frame_queue_test
    PRIVATE
        frame_pipeline
)

enable_project_warnings(frame_queue_test)

add_test(NAME frame_queue_test COMMAND frame_queue_test)

add_executable(frame_pipeline_test
    tests/frame_pipeline_test.cpp
)

target_include_directories(frame_pipeline_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(frame_pipeline_test
    PRIVATE
        frame_pipeline
)

enable_project_warnings(frame_pipeline_test)

add_test(NAME frame_pipeline_test COMMAND frame_pipeline_test)

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
