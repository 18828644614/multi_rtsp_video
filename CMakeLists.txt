cmake_minimum_required(VERSION 3.20)

project(multi_rtsp_video_analysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

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
