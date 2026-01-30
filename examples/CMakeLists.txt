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
