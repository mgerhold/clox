cmake_minimum_required(VERSION 3.28)
project(clox C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(project_options.cmake)

add_subdirectory(src bin)
