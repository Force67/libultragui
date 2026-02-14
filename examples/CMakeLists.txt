add_executable(ultragui_demo demo/main.cpp)
target_link_libraries(ultragui_demo PRIVATE ultragui glfw)
target_compile_definitions(ultragui_demo PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
)
add_dependencies(ultragui_demo ultragui_shaders)

add_executable(ultragui_showcase showcase/main.cpp)
target_link_libraries(ultragui_showcase PRIVATE ultragui glfw)
target_compile_definitions(ultragui_showcase PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    ULTRAGUI_SHOWCASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/showcase"
)
add_dependencies(ultragui_showcase ultragui_shaders)

add_executable(ultragui_anim_demo anim_demo/main.cpp)
target_link_libraries(ultragui_anim_demo PRIVATE ultragui glfw)
target_compile_definitions(ultragui_anim_demo PRIVATE
    ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    ULTRAGUI_ANIM_DEMO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/anim_demo"
)
add_dependencies(ultragui_anim_demo ultragui_shaders)

if(ULTRAGUI_LOTTIE)
    add_executable(ultragui_lottie_demo lottie_demo/main.cpp)
    target_link_libraries(ultragui_lottie_demo PRIVATE ultragui glfw rlottie)
    target_compile_definitions(ultragui_lottie_demo PRIVATE
        ULTRAGUI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
        ULTRAGUI_LOTTIE_DEMO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lottie_demo"
    )
    add_dependencies(ultragui_lottie_demo ultragui_shaders)
endif()
