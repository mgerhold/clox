include(${PROJECT_SOURCE_DIR}/cmake/warnings.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/sanitizers.cmake)

# the following function was taken from:
# https://github.com/cpp-best-practices/cmake_template/blob/main/ProjectOptions.cmake
macro(check_sanitizer_support)
    if ((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
        set(supports_ubsan ON)
    else ()
        set(supports_ubsan OFF)
    endif ()

    if ((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
        set(supports_asan OFF)
    else ()
        set(supports_asan ON)
    endif ()
endmacro()

if (PROJECT_IS_TOP_LEVEL)
    option(clox_warnings_as_errors "Treat warnings as errors" ON)
    option(clox_enable_undefined_behavior_sanitizer "Enable undefined behavior sanitizer" ${supports_ubsan})
    option(clox_enable_address_sanitizer "Enable address sanitizer" ${supports_asan})
    option(clox_build_tests "Build unit tests" ON)
else ()
    option(clox_warnings_as_errors "Treat warnings as errors" OFF)
    option(clox_enable_undefined_behavior_sanitizer "Enable undefined behavior sanitizer" OFF)
    option(clox_enable_address_sanitizer "Enable address sanitizer" OFF)
    option(clox_build_tests "Build unit tests" OFF)
endif ()
option(clox_build_shared_libs "Build shared libraries instead of static libraries" ON)
set(BUILD_SHARED_LIBS ${clox_build_shared_libs})

add_library(clox_warnings INTERFACE)
clox_set_warnings(clox_warnings ${clox_warnings_as_errors})

add_library(clox_sanitizers INTERFACE)
clox_enable_sanitizers(clox_sanitizers ${clox_enable_address_sanitizer} ${clox_enable_undefined_behavior_sanitizer})

add_library(clox_project_options INTERFACE)
target_link_libraries(clox_project_options
        INTERFACE clox_warnings
        INTERFACE clox_sanitizers
)
