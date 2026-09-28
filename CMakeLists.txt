cmake_minimum_required(VERSION 3.16)

project(e58_navlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(e58_navlib STATIC
    src/command_frame.cpp
    src/drone_controller.cpp
)

target_include_directories(e58_navlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(e58_example
    examples/send_example.cpp
)

target_link_libraries(e58_example PRIVATE e58_navlib)

enable_testing()

add_executable(e58_frame_test
    tests/frame_test.cpp
)

target_link_libraries(e58_frame_test PRIVATE e58_navlib)

add_test(NAME frame_matches_capture COMMAND e58_frame_test)
