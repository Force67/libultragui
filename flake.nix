{
  description = "libultragui: lean GPU-accelerated game GUI library";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # DXVK-native rebuilt with GLFW WSI (default nixpkgs build uses SDL3)
        dxvk-glfw = pkgs.dxvk_2.overrideAttrs (old: {
          mesonFlags = (old.mesonFlags or []) ++ [
            "-Dnative_glfw=enabled"
          ];
          buildInputs = (old.buildInputs or []) ++ [ pkgs.glfw ];
        });
      in
      {
        devShells.default = pkgs.mkShell {
          name = "ultragui-dev";

          nativeBuildInputs = with pkgs; [
            # Build tools
            cmake
            ninja
            pkg-config

            # C++ toolchain
            clang_18
            lldb_18
            clang-tools  # clang-format, clang-tidy

            # Shader compilation
            shaderc
            glslang
            directx-shader-compiler  # HLSL -> DXIL (D3D12 shaders)
          ];

          buildInputs = with pkgs; [
            # Graphics
            vulkan-headers
            vulkan-loader
            vulkan-validation-layers
            vulkan-tools

            # D3D12 via vkd3d (Linux testing)
            vkd3d

            # D3D11 via DXVK-native with GLFW WSI (Linux testing)
            dxvk-glfw

            # Windowing
            glfw

            # Text rendering
            freetype
            harfbuzz
            glib

            # Scripting
            lua5_4

            # Image loading (header-only, but useful to have)
            stb
          ];

          shellHook = ''
            export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
            export CC=clang
            export CXX=clang++
            echo "game libultragui dev shell: clang $(clang --version | head -1 | grep -oP '\d+\.\d+\.\d+'), cmake $(cmake --version | head -1 | grep -oP '\d+\.\d+\.\d+')"
          '';
        };
      }
    );
}
