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

        # DXVK-native: d3d11.so/dxgi.so that translate to Vulkan on this
        # machine. The GLFW WSI is compiled in and already dlopens nixpkgs'
        # glfw by store path, so the library and DXVK share one GLFW; pick it
        # at runtime with DXVK_WSI_DRIVER=GLFW.
        dxvk = pkgs.dxvk_2;

        # vkd3d, for the D3DCompile() in vkd3d-utils: the HLSL -> DXBC
        # compiler the D3D11 backend calls at startup. Its meta.platforms is
        # inherited from wine, which on aarch64 pulls in the i686 package set
        # and refuses to evaluate; the library itself is portable, so point it
        # at wine64 (a build-time widl provider) and widen the platform list.
        vkd3d = (pkgs.vkd3d.override { wine = pkgs.wine64; }).overrideAttrs
          (old: { meta = old.meta // { platforms = pkgs.lib.platforms.linux; }; });
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

            # D3D12 via vkd3d, D3D11 via DXVK-native (Linux testing).
            # vkd3d-utils also supplies D3DCompile for the D3D11 backend.
            vkd3d
            dxvk

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
            # DXVK-native dlopens libvulkan.so.1 and libvkd3d-utils.so.1 by
            # soname. On a distro they sit in the default search path; here
            # they only exist in the store, so name their directories.
            export LD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib:${pkgs.lib.getLib vkd3d}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            # DXVK-native has no default WSI; the D3D11 backend hands it a
            # GLFWwindow* as its HWND, so it has to use the GLFW backend.
            export DXVK_WSI_DRIVER=GLFW
            export CC=clang
            export CXX=clang++
            echo "game libultragui dev shell: clang $(clang --version | head -1 | grep -oP '\d+\.\d+\.\d+'), cmake $(cmake --version | head -1 | grep -oP '\d+\.\d+\.\d+')"
          '';
        };
      }
    );
}
