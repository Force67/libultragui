# libultragui

Lean, GPU-accelerated game GUI library. Direct rendering to Vulkan/D3D12 with a CSS-like declarative IDL and Flutter-inspired widget abstractions — purpose-built for AAA game UIs.

## Build

```bash
# Enter dev shell (NixOS)
nix develop

# Configure & build
cmake -B build -G Ninja
cmake --build build

# Run demo
./build/examples/ultragui_demo
```

## Status

Early development — Phase 0 (project skeleton).
