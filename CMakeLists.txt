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
option(ULTRAGUI_BACKEND_D3D11 "Enable D3D11 backend (DXVK-native)" OFF)
option(ULTRAGUI_BACKEND_OPENGL "Enable OpenGL 3.3 backend" OFF)
option(ULTRAGUI_AUDIO "Enable audio engine (miniaudio)" ON)
option(ULTRAGUI_LOTTIE "Enable Lottie animation support (rlottie)" ON)
option(ULTRAGUI_VIDEO "Enable MPEG-1 video playback (pl_mpeg)" ON)
option(ULTRAGUI_LUA "Enable built-in Lua scripting runtime" ON)
option(ULTRAGUI_DEFAULT_OPEN_URL "Compile a default Platform::OpenURL implementation" ON)
option(ULTRAGUI_BUILD_SHADERS "Compile the bundled GLSL shaders with glslc" ${ULTRAGUI_BACKEND_VULKAN})
# A platform with no window of its own, which the host feeds (see
# ugui/platform/platform_host.h): for embedding where the host keeps its
# window to itself, a game's say. Pairs with draw-data mode.
option(ULTRAGUI_PLATFORM_HOST "Use the host-driven platform instead of GLFW" OFF)
# A host that supplies its own ugui::Platform (ULTRAGUI_PLATFORM_SOURCE) and
# renders the draw list with its own pipelines does not need GLFW or the bundled
# GLSL shaders; turn these off to drop those build dependencies.
if(ULTRAGUI_PLATFORM_HOST)
    set(_default_platform_glfw OFF)
else()
    set(_default_platform_glfw ON)
endif()
option(ULTRAGUI_PLATFORM_GLFW "Build the bundled GLFW platform backend" ${_default_platform_glfw})
# Draw-data renderer and input backends compiled into the library (Dear ImGui
# style). ugui_impl_dx11 renders into a host's D3D11 device; ugui_impl_win32
# turns window messages into input for the host platform.
option(ULTRAGUI_IMPL_DX11 "Build the ugui_impl_dx11 draw-data backend" OFF)
if(WIN32 AND ULTRAGUI_PLATFORM_HOST)
    set(_default_impl_win32 ON)
else()
    set(_default_impl_win32 OFF)
endif()
option(ULTRAGUI_IMPL_WIN32 "Build the ugui_impl_win32 input backend" ${_default_impl_win32})
# Where pkg-config has nothing to offer, build freetype, harfbuzz and Lua from
# pinned sources (cmake/BundledDeps.cmake).
option(ULTRAGUI_BUNDLED_DEPS "Build freetype, harfbuzz and Lua from source" ${WIN32})
# Build against equilibrium base instead of the C++ standard library, and
# without exceptions (ugui/core/config.h). A parent project that already builds
# equilibrium::base supplies it; otherwise ULTRAGUI_EQUILIBRIUM_DIR names the
# checkout to build it from.
option(ULTRAGUI_USE_BASE "Use equilibrium base instead of the STL" OFF)
set(ULTRAGUI_EQUILIBRIUM_DIR ""
    CACHE PATH "equilibrium checkout used when ULTRAGUI_USE_BASE=ON and no equilibrium::base target exists")

# The equilibrium base headers require C++23.
if(ULTRAGUI_USE_BASE)
    set(CMAKE_CXX_STANDARD 23)
endif()

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
if(ULTRAGUI_BUNDLED_DEPS)
    include(cmake/BundledDeps.cmake)
endif()
# pkg-config is only asked for what nobody handed over.
if(NOT (TARGET freetype AND TARGET harfbuzz)
   OR (ULTRAGUI_LUA AND NOT TARGET ultragui_lua)
   OR ULTRAGUI_BACKEND_D3D12
   OR (NOT WIN32 AND (ULTRAGUI_BACKEND_D3D11 OR ULTRAGUI_IMPL_DX11)))
    find_package(PkgConfig REQUIRED)
endif()

if(ULTRAGUI_USE_BASE AND NOT TARGET equilibrium::base)
    if(NOT EXISTS "${ULTRAGUI_EQUILIBRIUM_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "ULTRAGUI_USE_BASE=ON needs an equilibrium checkout; set "
            "ULTRAGUI_EQUILIBRIUM_DIR (now ${ULTRAGUI_EQUILIBRIUM_DIR})")
    endif()
    set(EQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ULTRAGUI_EQUILIBRIUM_DIR}" equilibrium EXCLUDE_FROM_ALL)
endif()

if(ULTRAGUI_BACKEND_VULKAN OR ULTRAGUI_BACKEND_D3D12)
    find_package(Vulkan REQUIRED)
endif()

# Text stack. An embedder targeting a platform with no pkg-config to query (any
# cross build) can build freetype and harfbuzz itself and hand the targets over;
# otherwise they come from the system.
if(TARGET freetype AND TARGET harfbuzz)
    set(ULTRAGUI_TEXT_LIBS freetype harfbuzz)
    message(STATUS "ultragui: using the embedder's freetype/harfbuzz targets")
else()
    pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
    pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
    set(ULTRAGUI_TEXT_LIBS PkgConfig::FREETYPE PkgConfig::HARFBUZZ)
endif()
if(ULTRAGUI_LUA)
    if(TARGET ultragui_lua)
        set(ULTRAGUI_LUA_LIBS ultragui_lua)
    else()
        pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
        set(ULTRAGUI_LUA_LIBS PkgConfig::LUA)
    endif()
endif()

# GLFW (bundled platform backend + examples)
if(ULTRAGUI_PLATFORM_GLFW OR ULTRAGUI_BUILD_EXAMPLES)
    find_package(glfw3 3.3 REQUIRED)
endif()

# Yoga layout engine (built from source). Add the library subdir directly, not
# the yoga root: the root unconditionally adds a GoogleTest-based tests target.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yoga/yoga EXCLUDE_FROM_ALL)

# rlottie (Lottie animation renderer)
if(ULTRAGUI_LOTTIE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(LOTTIE_MODULE OFF CACHE BOOL "" FORCE)
    set(LOTTIE_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/rlottie EXCLUDE_FROM_ALL)
endif()

# --- Shader compilation ---
if(ULTRAGUI_BUILD_SHADERS)
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
endif()  # ULTRAGUI_BUILD_SHADERS

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

# --- HLSL source embedding (D3D11 and ugui_impl_dx11) ---
# D3D11 consumes DXBC, which dxc no longer emits, so the backend compiles the
# HLSL at startup. Embedding the shader text keeps shaders/hlsl as the single
# source for both D3D backends, and leaves the D3D11 path with no shader files
# to ship.
if(ULTRAGUI_BACKEND_D3D11 OR ULTRAGUI_IMPL_DX11)
    set(HLSL_EMBED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set(HLSL_EMBED_HEADER ${HLSL_EMBED_DIR}/ugui_hlsl_embedded.h)
    file(MAKE_DIRECTORY ${HLSL_EMBED_DIR})
    add_custom_command(
        OUTPUT ${HLSL_EMBED_HEADER}
        COMMAND ${CMAKE_COMMAND}
                -DHLSL_DIR=${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl
                -DOUT=${HLSL_EMBED_HEADER}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedHlsl.cmake
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/quad.hlsl
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/text.hlsl
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/video.hlsl
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedHlsl.cmake
        COMMENT "Embedding HLSL sources for the D3D11 backends"
    )
    add_custom_target(ultragui_hlsl_embed DEPENDS ${HLSL_EMBED_HEADER})
endif()

# --- Swappable implementation sources ---
# Point these at your own .cc file to provide a custom backend.
if(ULTRAGUI_PLATFORM_HOST)
    set(_default_platform_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/platform/platform_host.cc")
else()
    set(_default_platform_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/platform/platform_glfw.cc")
endif()
set(ULTRAGUI_PLATFORM_SOURCE "${_default_platform_src}"
    CACHE FILEPATH "Platform implementation source file")
# The library compiles exactly one RHI translation unit, because they all
# define ugui::RHI. The backend switches pick it, most specific first. Vulkan
# comes last: ULTRAGUI_BACKEND_VULKAN stays ON unless someone turns it off.
if(ULTRAGUI_BACKEND_D3D11)
    set(_default_rhi_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/rhi/d3d11/d3d11_rhi.cc")
elseif(ULTRAGUI_BACKEND_D3D12)
    set(_default_rhi_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/rhi/d3d12/d3d12_rhi.cc")
elseif(ULTRAGUI_BACKEND_OPENGL AND NOT ULTRAGUI_BACKEND_VULKAN)
    set(_default_rhi_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/rhi/opengl/opengl_rhi.cc")
elseif(NOT ULTRAGUI_BACKEND_VULKAN)
    # No backend at all: a host that renders the draw data itself.
    set(_default_rhi_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/rhi/null/null_rhi.cc")
else()
    set(_default_rhi_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/rhi/vulkan/vulkan_rhi.cc")
endif()
set(ULTRAGUI_RHI_SOURCE "${_default_rhi_src}"
    CACHE FILEPATH "RHI implementation source file")
if(ULTRAGUI_LUA)
    set(_default_script_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/scripting/lua_runtime.cc")
else()
    set(_default_script_src "${CMAKE_CURRENT_SOURCE_DIR}/ugui/scripting/script_runtime_none.cc")
endif()
set(ULTRAGUI_SCRIPT_SOURCE "${_default_script_src}"
    CACHE FILEPATH "Script runtime implementation source file")

# --- Core library ---
set(ULTRAGUI_SOURCES
    ugui/core/arena.cc
    ugui/core/from_chars.cc
    ugui/core/string_utils.cc
    ugui/render/renderer2d.cc
    ugui/style/style.cc
    ugui/style/transition.cc
    ugui/text/text_engine.cc
    ugui/layout/layout.cc
    ugui/animation/animator.cc
    ugui/widgets/widget.cc
    ugui/widgets/widget_registry.cc
    ugui/widgets/widget_vtable.cc
    ugui/widgets/panel.cc
    ugui/widgets/text.cc
    ugui/widgets/text_input.cc
    ugui/widgets/checkbox.cc
    ugui/widgets/radio.cc
    ugui/widgets/toggle.cc
    ugui/widgets/slider.cc
    ugui/widgets/button.cc
    ugui/widgets/context_menu.cc
    ugui/widgets/dropdown.cc
    ugui/widgets/image.cc
    ugui/widgets/message_box.cc
    ugui/widgets/modal.cc
    ugui/widgets/rich_text.cc
    ugui/widgets/scroll_view.cc
    ugui/widgets/widget_tree.cc
    ugui/idl/parser.cc
    ugui/idl/builder.cc
    ugui/input/input.cc
    ugui/scripting/lua_widgets.cc
    ugui/anim/json.cc
    ugui/anim/anim_parser.cc
    ugui/anim/anim_eval.cc
    ugui/anim/anim_render.cc
    ugui/anim/vector_animation.cc
    ugui/layout/layout_tree.cc
    ugui/render/paint.cc
    ugui/svg/svg.cc
    ugui/svg/svg_parser.cc
    ugui/svg/svg_rasterizer.cc
    ugui/ui_context.cc
)

list(APPEND ULTRAGUI_SOURCES ${ULTRAGUI_PLATFORM_SOURCE})
list(APPEND ULTRAGUI_SOURCES ${ULTRAGUI_RHI_SOURCE})
list(APPEND ULTRAGUI_SOURCES ${ULTRAGUI_SCRIPT_SOURCE})

if(ULTRAGUI_BACKEND_D3D11 OR ULTRAGUI_IMPL_DX11)
    list(APPEND ULTRAGUI_SOURCES ugui/rhi/d3d11/d3d_shader_compiler.cc)
endif()
if(ULTRAGUI_IMPL_DX11)
    list(APPEND ULTRAGUI_SOURCES ugui/backends/ugui_impl_dx11.cc)
endif()
if(ULTRAGUI_IMPL_WIN32)
    list(APPEND ULTRAGUI_SOURCES ugui/backends/ugui_impl_win32.cc)
endif()

if(ULTRAGUI_LUA)
    list(APPEND ULTRAGUI_SOURCES
        ugui/scripting/lua_anim.cc
    )
endif()

if(ULTRAGUI_DEFAULT_OPEN_URL)
    list(APPEND ULTRAGUI_SOURCES ugui/platform/platform_open_url.cc)
endif()

if(ULTRAGUI_AUDIO)
    # miniaudio's implementation (one TU). Linked by the video audio bridge and
    # by the wire-in miniaudio backend (ugui/backends/ugui_impl_miniaudio.cc),
    # which applications compile into their own target (Dear ImGui style).
    list(APPEND ULTRAGUI_SOURCES ugui/audio/miniaudio_impl.cc)
    if(ULTRAGUI_LUA)
        list(APPEND ULTRAGUI_SOURCES ugui/scripting/lua_audio.cc)
    endif()
endif()

if(ULTRAGUI_LOTTIE)
    list(APPEND ULTRAGUI_SOURCES ugui/lottie/lottie.cc)
    if(ULTRAGUI_LUA)
        list(APPEND ULTRAGUI_SOURCES ugui/scripting/lua_lottie.cc)
    endif()
endif()

if(ULTRAGUI_VIDEO)
    list(APPEND ULTRAGUI_SOURCES ugui/video/video.cc)
    if(ULTRAGUI_LUA)
        list(APPEND ULTRAGUI_SOURCES ugui/scripting/lua_video.cc)
    endif()
endif()

add_library(ultragui STATIC ${ULTRAGUI_SOURCES})
if(ULTRAGUI_BUILD_SHADERS)
    add_dependencies(ultragui ultragui_shaders)
endif()
if(ULTRAGUI_BACKEND_D3D12)
    add_dependencies(ultragui ultragui_hlsl_shaders)
endif()
if(ULTRAGUI_BACKEND_D3D11 OR ULTRAGUI_IMPL_DX11)
    add_dependencies(ultragui ultragui_hlsl_embed)
    target_include_directories(ultragui PRIVATE ${HLSL_EMBED_DIR})
endif()

target_include_directories(ultragui
    PUBLIC
        # Headers and sources are co-located under ugui/, so the include root is
        # the project root and headers resolve as <ugui/...>.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:.>
    PRIVATE
        $<$<BOOL:${ULTRAGUI_AUDIO}>:${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio>
        $<$<BOOL:${ULTRAGUI_VIDEO}>:${CMAKE_CURRENT_SOURCE_DIR}/third_party/pl_mpeg>
)

target_compile_definitions(ultragui PRIVATE
    ULTRAGUI_SHADER_DIR="${SHADER_OUT_DIR}"
    # The portable C file API (fopen and friends) is what the library uses.
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

# Public-API export macro (ugui/core/export.h). No-op for the static library;
# when ultragui is built SHARED, UGUI_SHARED is seen by everyone and UGUI_BUILD
# only while compiling the library, so annotated symbols are exported/imported.
target_compile_definitions(ultragui
    PUBLIC  $<$<STREQUAL:$<TARGET_PROPERTY:ultragui,TYPE>,SHARED_LIBRARY>:UGUI_SHARED>
    PRIVATE $<$<STREQUAL:$<TARGET_PROPERTY:ultragui,TYPE>,SHARED_LIBRARY>:UGUI_BUILD>
)

if(ULTRAGUI_USE_BASE)
    target_link_libraries(ultragui PUBLIC equilibrium::base)
    target_compile_definitions(ultragui PUBLIC ULTRAGUI_USE_BASE)
    # base's std::formatter interop would pull <format> into every TU that
    # names a ugui::String; the library itself never formats with it.
    target_compile_definitions(ultragui PRIVATE BASE_NO_STD_FORMAT)
    # base's rules include no exceptions; the vendored C++ dependencies are
    # separate targets and keep their own flags.
    target_compile_options(ultragui PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/EHs-c-,-fno-exceptions>)
endif()

target_link_libraries(ultragui
    PRIVATE
        ultragui_warnings
        ${ULTRAGUI_TEXT_LIBS}
        $<$<BOOL:${ULTRAGUI_LUA}>:${ULTRAGUI_LUA_LIBS}>
        $<$<BOOL:${ULTRAGUI_PLATFORM_GLFW}>:glfw>
        yogacore
)

# Feature flags: always defined to 0/1 so the built library and the public
# headers (include/ultragui/ultragui_config.h) always agree. The option(...)
# switches above are authoritative.
target_compile_definitions(ultragui PUBLIC
    ULTRAGUI_BACKEND_VULKAN=$<BOOL:${ULTRAGUI_BACKEND_VULKAN}>
    ULTRAGUI_BACKEND_D3D12=$<BOOL:${ULTRAGUI_BACKEND_D3D12}>
    ULTRAGUI_BACKEND_D3D11=$<BOOL:${ULTRAGUI_BACKEND_D3D11}>
    ULTRAGUI_BACKEND_OPENGL=$<BOOL:${ULTRAGUI_BACKEND_OPENGL}>
    ULTRAGUI_LUA=$<BOOL:${ULTRAGUI_LUA}>
    ULTRAGUI_AUDIO=$<BOOL:${ULTRAGUI_AUDIO}>
    ULTRAGUI_LOTTIE=$<BOOL:${ULTRAGUI_LOTTIE}>
    ULTRAGUI_VIDEO=$<BOOL:${ULTRAGUI_VIDEO}>
    ULTRAGUI_DEFAULT_OPEN_URL=$<BOOL:${ULTRAGUI_DEFAULT_OPEN_URL}>
    ULTRAGUI_PLATFORM_HOST=$<BOOL:${ULTRAGUI_PLATFORM_HOST}>
)

if(ULTRAGUI_BACKEND_VULKAN)
    target_link_libraries(ultragui PRIVATE Vulkan::Vulkan)
endif()

if(ULTRAGUI_BACKEND_D3D12)
    pkg_check_modules(VKD3D REQUIRED IMPORTED_TARGET libvkd3d)
    pkg_check_modules(VKD3D_UTILS REQUIRED IMPORTED_TARGET libvkd3d-utils)
    target_link_libraries(ultragui PRIVATE PkgConfig::VKD3D PkgConfig::VKD3D_UTILS Vulkan::Vulkan)
    target_compile_definitions(ultragui PRIVATE
        ULTRAGUI_HLSL_SHADER_DIR="${HLSL_OUT_DIR}"
    )
endif()

if(ULTRAGUI_BACKEND_D3D11)
    if(WIN32)
        # d3d11 and dxgi come from the platform SDK. The HLSL compiler is
        # loaded at run time out of d3dcompiler_47.dll (which ships with
        # Windows), so it needs no import library here.
        target_link_libraries(ultragui PRIVATE d3d11 dxgi)
    else()
        # DXVK-native stands in for d3d11/dxgi, and vkd3d-utils supplies the
        # D3DCompile the backend dlopens. Run with DXVK_WSI_DRIVER=GLFW.
        pkg_check_modules(DXVK_D3D11 REQUIRED IMPORTED_TARGET dxvk-d3d11)
        pkg_check_modules(DXVK_DXGI REQUIRED IMPORTED_TARGET dxvk-dxgi)
        pkg_check_modules(VKD3D_UTILS_D3D11 REQUIRED IMPORTED_TARGET libvkd3d-utils)
        target_link_libraries(ultragui PRIVATE PkgConfig::DXVK_D3D11 PkgConfig::DXVK_DXGI PkgConfig::VKD3D_UTILS_D3D11)
    endif()
endif()

if(ULTRAGUI_IMPL_DX11 AND NOT WIN32 AND NOT ULTRAGUI_BACKEND_D3D11)
    # The backend only calls through the host's device, so it needs DXVK's
    # headers and nothing to link; vkd3d-utils is dlopened for D3DCompile.
    pkg_check_modules(DXVK_D3D11_HEADERS REQUIRED IMPORTED_TARGET dxvk-d3d11)
    target_include_directories(ultragui SYSTEM PRIVATE ${DXVK_D3D11_HEADERS_INCLUDE_DIRS})
endif()

if(ULTRAGUI_BACKEND_OPENGL)
    find_package(OpenGL REQUIRED)
    target_link_libraries(ultragui PRIVATE OpenGL::GL)
endif()

if(ULTRAGUI_DEFAULT_OPEN_URL AND WIN32)
    target_link_libraries(ultragui PRIVATE shell32)
endif()

if(ULTRAGUI_LOTTIE)
    target_link_libraries(ultragui PRIVATE rlottie)
endif()

if(ULTRAGUI_VIDEO)
    # pl_mpeg is header-only, no linking needed.
    # When ULTRAGUI_AUDIO is also ON, video.cc includes miniaudio.h for audio
    # bridge -- the include path is already provided by the AUDIO block below.
    # When AUDIO is OFF, video plays silently (no audio deps).
endif()

if(ULTRAGUI_AUDIO)
    # miniaudio uses dlopen for audio backends on Linux, pthread for threading.
    # Propagated to consumers (incl. the wire-in miniaudio backend) since this
    # is a static library.
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
    if(ULTRAGUI_USE_BASE)
        message(STATUS "ultragui: examples use the STL; skipped with ULTRAGUI_USE_BASE")
    else()
        add_subdirectory(examples)
    endif()
endif()

# --- Tests ---
if(ULTRAGUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
