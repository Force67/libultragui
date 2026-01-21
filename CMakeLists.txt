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

# --- Shader compilation ---
find_program(GLSLC glslc REQUIRED)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES
    ${SHADER_DIR}/quad.vert
    ${SHADER_DIR}/quad.frag
    ${SHADER_DIR}/text.vert
    ${SHADER_DIR}/text.frag
)

set(SHADER_OUTPUTS)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_OUTPUT ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPV_OUTPUT}
        COMMAND ${GLSLC} ${SHADER} -o ${SPV_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND SHADER_OUTPUTS ${SPV_OUTPUT})
endforeach()

add_custom_target(ultragui_shaders DEPENDS ${SHADER_OUTPUTS})

# --- Core library ---
set(ULTRAGUI_SOURCES
    src/core/arena.cpp
    src/core/string_utils.cpp
    src/platform/platform_glfw.cpp
    src/render/renderer2d.cpp
    src/style/style.cpp
    src/style/transition.cpp
    src/text/text_engine.cpp
    src/layout/layout.cpp
    src/animation/animator.cpp
    src/widgets/widget.cpp
    src/widgets/panel.cpp
    src/widgets/text.cpp
    src/widgets/button.cpp
    src/widgets/image.cpp
    src/widgets/scroll_view.cpp
)

if(ULTRAGUI_BACKEND_VULKAN)
    list(APPEND ULTRAGUI_SOURCES src/rhi/vulkan/vulkan_rhi.cpp)
endif()

add_library(ultragui STATIC ${ULTRAGUI_SOURCES})
add_dependencies(ultragui ultragui_shaders)

target_include_directories(ultragui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(ultragui PRIVATE
    ULTRAGUI_SHADER_DIR="${SHADER_OUT_DIR}"
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
