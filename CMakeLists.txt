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
option(ULTRAGUI_AUDIO "Enable audio engine (miniaudio)" ON)
option(ULTRAGUI_LOTTIE "Enable Lottie animation support (rlottie)" ON)
option(ULTRAGUI_VIDEO "Enable MPEG-1 video playback (pl_mpeg)" ON)
option(ULTRAGUI_LUA "Enable built-in Lua scripting runtime" ON)

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

if(ULTRAGUI_BACKEND_VULKAN OR ULTRAGUI_BACKEND_D3D12)
    find_package(Vulkan REQUIRED)
endif()

pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
if(ULTRAGUI_LUA)
    pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
endif()

# GLFW (for examples/platform)
find_package(glfw3 3.3 REQUIRED)

# Yoga layout engine (built from source)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yoga EXCLUDE_FROM_ALL)

# rlottie (Lottie animation renderer)
if(ULTRAGUI_LOTTIE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(LOTTIE_MODULE OFF CACHE BOOL "" FORCE)
    set(LOTTIE_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/rlottie EXCLUDE_FROM_ALL)
endif()

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
    ${SHADER_DIR}/video.vert
    ${SHADER_DIR}/video.frag
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

# --- HLSL shader compilation (D3D12) ---
if(ULTRAGUI_BACKEND_D3D12)
    find_program(DXC dxc REQUIRED)
    set(HLSL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl)
    set(HLSL_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders/hlsl)
    file(MAKE_DIRECTORY ${HLSL_OUT_DIR})

    set(HLSL_SHADERS
        quad text video
    )
    set(HLSL_OUTPUTS)
    foreach(HLSL_NAME ${HLSL_SHADERS})
        set(HLSL_FILE ${HLSL_DIR}/${HLSL_NAME}.hlsl)
        set(VS_OUTPUT ${HLSL_OUT_DIR}/${HLSL_NAME}_vs.cso)
        set(PS_OUTPUT ${HLSL_OUT_DIR}/${HLSL_NAME}_ps.cso)
        add_custom_command(
            OUTPUT ${VS_OUTPUT}
            COMMAND ${DXC} -T vs_6_0 -E VSMain -Fo ${VS_OUTPUT} ${HLSL_FILE}
            DEPENDS ${HLSL_FILE}
            COMMENT "Compiling HLSL VS: ${HLSL_NAME}"
        )
        add_custom_command(
            OUTPUT ${PS_OUTPUT}
            COMMAND ${DXC} -T ps_6_0 -E PSMain -Fo ${PS_OUTPUT} ${HLSL_FILE}
            DEPENDS ${HLSL_FILE}
            COMMENT "Compiling HLSL PS: ${HLSL_NAME}"
        )
        list(APPEND HLSL_OUTPUTS ${VS_OUTPUT} ${PS_OUTPUT})
    endforeach()

    add_custom_target(ultragui_hlsl_shaders DEPENDS ${HLSL_OUTPUTS})
endif()

# --- Swappable implementation sources ---
# Point these at your own .cc file to provide a custom backend.
set(ULTRAGUI_PLATFORM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/platform_glfw.cc"
    CACHE FILEPATH "Platform implementation source file")
set(ULTRAGUI_RHI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/vulkan/vulkan_rhi.cc"
    CACHE FILEPATH "RHI implementation source file")
if(ULTRAGUI_LUA)
    set(_default_script_src "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_runtime.cc")
else()
    set(_default_script_src "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/script_runtime_none.cc")
endif()
set(ULTRAGUI_SCRIPT_SOURCE "${_default_script_src}"
    CACHE FILEPATH "Script runtime implementation source file")

# --- Core library ---
set(ULTRAGUI_SOURCES
    src/core/arena.cc
    src/core/string_utils.cc
    src/render/renderer2d.cc
    src/style/style.cc
    src/style/transition.cc
    src/text/text_engine.cc
    src/layout/layout.cc
    src/animation/animator.cc
    src/widgets/widget.cc
    src/widgets/panel.cc
    src/widgets/text.cc
    src/widgets/text_input.cc
    src/widgets/checkbox.cc
    src/widgets/radio.cc
    src/widgets/toggle.cc
    src/widgets/slider.cc
    src/widgets/button.cc
    src/widgets/context_menu.cc
    src/widgets/dropdown.cc
    src/widgets/image.cc
    src/widgets/modal.cc
    src/widgets/rich_text.cc
    src/widgets/scroll_view.cc
    src/widgets/widget_tree.cc
    src/idl/parser.cc
    src/idl/builder.cc
    src/input/input.cc
    src/scripting/lua_widgets.cc
    src/anim/json.cc
    src/anim/anim_parser.cc
    src/anim/anim_eval.cc
    src/anim/anim_render.cc
    src/anim/vector_animation.cc
    src/layout/layout_tree.cc
    src/render/paint.cc
    src/svg/svg.cc
    src/svg/svg_parser.cc
    src/svg/svg_rasterizer.cc
    src/ui_context.cc
)

list(APPEND ULTRAGUI_SOURCES ${ULTRAGUI_PLATFORM_SOURCE})
list(APPEND ULTRAGUI_SOURCES ${ULTRAGUI_RHI_SOURCE})
list(APPEND ULTRAGUI_SOURCES ${ULTRAGUI_SCRIPT_SOURCE})

if(ULTRAGUI_LUA)
    list(APPEND ULTRAGUI_SOURCES
        src/scripting/lua_anim.cc
    )
endif()

if(ULTRAGUI_AUDIO)
    list(APPEND ULTRAGUI_SOURCES src/audio/audio.cc)
    if(ULTRAGUI_LUA)
        list(APPEND ULTRAGUI_SOURCES src/scripting/lua_audio.cc)
    endif()
endif()

if(ULTRAGUI_LOTTIE)
    list(APPEND ULTRAGUI_SOURCES src/lottie/lottie.cc)
    if(ULTRAGUI_LUA)
        list(APPEND ULTRAGUI_SOURCES src/scripting/lua_lottie.cc)
    endif()
endif()

if(ULTRAGUI_VIDEO)
    list(APPEND ULTRAGUI_SOURCES src/video/video.cc)
    if(ULTRAGUI_LUA)
        list(APPEND ULTRAGUI_SOURCES src/scripting/lua_video.cc)
    endif()
endif()

add_library(ultragui STATIC ${ULTRAGUI_SOURCES})
add_dependencies(ultragui ultragui_shaders)
if(ULTRAGUI_BACKEND_D3D12)
    add_dependencies(ultragui ultragui_hlsl_shaders)
endif()

target_include_directories(ultragui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        $<$<BOOL:${ULTRAGUI_AUDIO}>:${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio>
        $<$<BOOL:${ULTRAGUI_VIDEO}>:${CMAKE_CURRENT_SOURCE_DIR}/third_party/pl_mpeg>
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
        $<$<BOOL:${ULTRAGUI_LUA}>:PkgConfig::LUA>
        glfw
        yogacore
)

if(ULTRAGUI_BACKEND_VULKAN)
    target_link_libraries(ultragui PRIVATE Vulkan::Vulkan)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_BACKEND_VULKAN=1)
endif()

if(ULTRAGUI_BACKEND_D3D12)
    pkg_check_modules(VKD3D REQUIRED IMPORTED_TARGET libvkd3d)
    pkg_check_modules(VKD3D_UTILS REQUIRED IMPORTED_TARGET libvkd3d-utils)
    target_link_libraries(ultragui PRIVATE PkgConfig::VKD3D PkgConfig::VKD3D_UTILS Vulkan::Vulkan)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_BACKEND_D3D12=1)
    target_compile_definitions(ultragui PRIVATE
        ULTRAGUI_HLSL_SHADER_DIR="${HLSL_OUT_DIR}"
    )
endif()

if(ULTRAGUI_LUA)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_LUA=1)
endif()

if(ULTRAGUI_LOTTIE)
    target_link_libraries(ultragui PRIVATE rlottie)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_LOTTIE=1)
endif()

if(ULTRAGUI_VIDEO)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_VIDEO=1)
    # pl_mpeg is header-only, no linking needed.
    # When ULTRAGUI_AUDIO is also ON, video.cc includes miniaudio.h for audio
    # bridge -- the include path is already provided by the AUDIO block below.
    # When AUDIO is OFF, video plays silently (no audio deps).
endif()

if(ULTRAGUI_AUDIO)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_AUDIO=1)
    # miniaudio uses dlopen for audio backends on Linux, pthread for threading
    if(UNIX AND NOT APPLE)
        target_link_libraries(ultragui PRIVATE pthread m dl)
    elseif(APPLE)
        find_library(COREAUDIO_LIB CoreAudio)
        find_library(AUDIOUNIT_LIB AudioUnit)
        find_library(COREFOUNDATION_LIB CoreFoundation)
        target_link_libraries(ultragui PRIVATE ${COREAUDIO_LIB} ${AUDIOUNIT_LIB} ${COREFOUNDATION_LIB})
    endif()
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
