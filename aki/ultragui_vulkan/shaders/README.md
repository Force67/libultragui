# Embedded SPIR-V

These are the repository's GLSL shaders (`shaders/*.vert`, `shaders/*.frag`
at the root) compiled to SPIR-V, byte for byte what the C++ build puts in its
shader_dir (`glslc <src> -o <name>.spv`, CMakeLists.txt). The Vulkan backends
read them from `RhiConfig::shader_dir` / `InitInfo::shader_dir` as the C++
does; with the package feature `embed_shaders` on
(`akiko build --feature ultragui_vulkan/embed_shaders`) they use these copies,
which `src/shaders.aki` `@embed`s, and need no shader directory at run time.

Regenerate after a shader changes:

    nix develop -c aki/ultragui_vulkan/tools/build_shaders.sh
