cmake_minimum_required(VERSION 3.20)

project(lanerunner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lanerunner src/main.cpp)

# -O3 is for speed
target_compile_options(lanerunner PRIVATE -O3 -Wall -Wextra -Wpedantic)

option(LOGOS_ASAN "Enable AddressSanitizer" OFF)
option(LOGOS_TSAN "Enable ThreadSanitizer" OFF)

if (LOGOS_ASAN)
  target_compile_options(lanerunner PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(lanerunner PRIVATE -fsanitize=address)
endif()

if (LOGOS_TSAN)
  target_compile_options(logos_simd PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(logos_simd PRIVATE -fsanitize=thread)
endif()
