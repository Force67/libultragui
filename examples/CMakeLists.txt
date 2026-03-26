add_executable(ultragui_demo demo/main.cc)
target_link_libraries(ultragui_demo PRIVATE ultragui glfw)
target_compile_definitions(ultragui_demo PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
)
add_dependencies(ultragui_demo ultragui_shaders)

add_executable(ultragui_showcase showcase/main.cc)
target_link_libraries(ultragui_showcase PRIVATE ultragui glfw)
target_compile_definitions(ultragui_showcase PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    ULTRAGUI_SHOWCASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/showcase"
)
add_dependencies(ultragui_showcase ultragui_shaders)

add_executable(ultragui_anim_demo anim_demo/main.cc)
target_link_libraries(ultragui_anim_demo PRIVATE ultragui glfw)
target_compile_definitions(ultragui_anim_demo PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    ULTRAGUI_ANIM_DEMO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/anim_demo"
)
add_dependencies(ultragui_anim_demo ultragui_shaders)

add_executable(ultragui_game_overlay game_overlay/main.cc)
target_link_libraries(ultragui_game_overlay PRIVATE ultragui glfw)
target_compile_definitions(ultragui_game_overlay PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    ULTRAGUI_GAME_OVERLAY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/game_overlay"
)
add_dependencies(ultragui_game_overlay ultragui_shaders)

# Embedding sample (Dear ImGui style): host owns the Vulkan device, swapchain
# and render pass; ultragui produces a draw list (ui.RenderDrawData) that the
# ugui_impl_vulkan backend records into the host's command buffer.
if(ULTRAGUI_BACKEND_VULKAN)
    add_executable(ultragui_embed_vulkan
        embed_vulkan/main.cc
        ${CMAKE_SOURCE_DIR}/ugui/backends/ugui_impl_vulkan.cc)
    target_link_libraries(ultragui_embed_vulkan PRIVATE ultragui glfw Vulkan::Vulkan)
    target_compile_definitions(ultragui_embed_vulkan PRIVATE
        ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders")
    add_dependencies(ultragui_embed_vulkan ultragui_shaders)
endif()

# Embedding sample (Dear ImGui style) on OpenGL 3.3 via draw data. Works with
# any library RHI backend, since draw-data mode uses no RHI of its own.
find_package(OpenGL REQUIRED)
add_executable(ultragui_embed_opengl3
    embed_opengl3/main.cc
    ${CMAKE_SOURCE_DIR}/ugui/backends/ugui_impl_opengl3.cc)
target_link_libraries(ultragui_embed_opengl3 PRIVATE ultragui glfw OpenGL::GL)

# Embedding sample: host owns the window + GL pipeline, ultragui composites on
# top. Requires the OpenGL backend (host shares its GL context with the RHI).
if(ULTRAGUI_BACKEND_OPENGL)
    find_package(OpenGL REQUIRED)
    add_executable(ultragui_embed_gl embed_gl/main.cc)
    target_link_libraries(ultragui_embed_gl PRIVATE ultragui glfw OpenGL::GL)
endif()

if(ULTRAGUI_VIDEO)
    add_executable(ultragui_video_demo video_demo/main.cc)
    target_link_libraries(ultragui_video_demo PRIVATE ultragui glfw)
    target_compile_definitions(ultragui_video_demo PRIVATE
        ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
        ULTRAGUI_VIDEO_DEMO_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_dependencies(ultragui_video_demo ultragui_shaders)
endif()

if(ULTRAGUI_LOTTIE)
    add_executable(ultragui_lottie_demo lottie_demo/main.cc)
    target_link_libraries(ultragui_lottie_demo PRIVATE ultragui glfw rlottie)
    target_compile_definitions(ultragui_lottie_demo PRIVATE
        ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
        ULTRAGUI_LOTTIE_DEMO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lottie_demo"
    )
    add_dependencies(ultragui_lottie_demo ultragui_shaders)
endif()
