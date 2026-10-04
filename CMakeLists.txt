cmake_minimum_required(VERSION 3.20)

project(multi_rtsp_video_analysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(multi_rtsp_video_analysis
    src/main.cpp
)

target_include_directories(multi_rtsp_video_analysis
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

enable_testing()

add_executable(smoke_test
    tests/smoke_test.cpp
)

target_include_directories(smoke_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME smoke_test COMMAND smoke_test)
