cmake_minimum_required(VERSION 3.20)
project(ultragui
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Lean GPU-accelerated game GUI library"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ULTRAGUI_BUILD_EXAMPLES "Build example applications" ON)
option(ULTRAGUI_BUILD_TESTS "Build tests" ON)
option(ULTRAGUI_BACKEND_VULKAN "Enable Vulkan backend" ON)
option(ULTRAGUI_BACKEND_D3D12 "Enable D3D12 backend" OFF)

# Compiler warnings
add_library(ultragui_warnings INTERFACE)
target_compile_options(ultragui_warnings INTERFACE
    $<$<CXX_COMPILER_ID:Clang,GNU>:
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Werror=return-type
        -Werror=uninitialized
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive-
    >
)

# --- Dependencies ---
find_package(PkgConfig REQUIRED)

if(ULTRAGUI_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)

# GLFW (for examples/platform)
find_package(glfw3 3.3 REQUIRED)

# --- Core library ---
add_library(ultragui STATIC
    src/core/arena.cpp
    src/core/string_utils.cpp
    src/platform/platform_glfw.cpp
)

target_include_directories(ultragui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ultragui
    PUBLIC
        ultragui_warnings
    PRIVATE
        PkgConfig::FREETYPE
        PkgConfig::HARFBUZZ
        PkgConfig::LUA
        glfw
)

if(ULTRAGUI_BACKEND_VULKAN)
    target_link_libraries(ultragui PRIVATE Vulkan::Vulkan)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_BACKEND_VULKAN=1)
endif()

# --- Examples ---
if(ULTRAGUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# --- Tests ---
if(ULTRAGUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
