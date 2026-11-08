#!/usr/bin/env bash
# Compiles shaders/hlsl/{quad,text,video}.hlsl to DXIL with dxc, as the C++
# build does (CMakeLists.txt, "HLSL shader compilation (D3D12)"), into
# shaders/*.cso, which src/shaders.aki embeds. Run inside the dev shell (dxc).
set -eu
cd "$(dirname "$0")/.."
for n in quad text video; do
  dxc -T vs_6_0 -E VSMain -Fo shaders/${n}_vs.cso ../../shaders/hlsl/${n}.hlsl
  dxc -T ps_6_0 -E PSMain -Fo shaders/${n}_ps.cso ../../shaders/hlsl/${n}.hlsl
done
ls -l shaders
