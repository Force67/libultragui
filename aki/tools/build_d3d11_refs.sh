#!/usr/bin/env bash
# build_d3d11_refs.sh [dir]: builds the C++ references the ultragui_d3d11 tests
# are compared with: <dir>/rhi_scene_d11 (the scene through the C++ D3D11 RHI,
# tools/rhi_scene.cc) and <dir>/draw_data_scene_dx11 (the scene as draw data
# through ugui_impl_dx11, tools/draw_data_scene_dx11.cc). Run from the repo root
# in the dev shell: nix develop -c aki/tools/build_d3d11_refs.sh /tmp/ugui-ref-d11
#
# ugui/backends/ugui_impl_dx11.cc does not compile in the repository (it calls
# std::unordered_map members, find()->second and erase(iterator), on ugui::HashMap), so the
# reference is built from a copy with those four spots changed to HashMap's API.
set -eu
d=${1:-/tmp/ugui-ref-d11}
cmake -S . -B "$d" -G Ninja -DULTRAGUI_BACKEND_D3D11=ON -DULTRAGUI_BACKEND_VULKAN=OFF -DULTRAGUI_BUILD_SHADERS=ON \
  -DULTRAGUI_AUDIO=OFF -DULTRAGUI_LOTTIE=OFF -DULTRAGUI_VIDEO=OFF -DULTRAGUI_LUA=OFF -DULTRAGUI_BUILD_TESTS=OFF >/dev/null
ninja -C "$d" ultragui
python3 - "$d" <<'PY'
import sys
s = open('ugui/backends/ugui_impl_dx11.cc').read()
def r(a, b):
    global s
    assert a in s, a
    s = s.replace(a, b)
r('''  auto it = g.textures.find(command.texture_id);
  return it != g.textures.end() ? &it->second : &g.white;''', '''  Texture* it = g.textures.find(command.texture_id);
  return it ? it : &g.white;''')
r('for (auto& entry : g.textures) FreeTexture(entry.second);', 'for (auto [id, t] : g.textures) FreeTexture(t);')
r('''  auto it = g.textures.find(id);
  if (it == g.textures.end() || !pixels) return;
  const Texture& t = it->second;''', '''  Texture* it = g.textures.find(id);
  if (!it || !pixels) return;
  const Texture& t = *it;''')
r('''  auto it = g.textures.find(id);
  if (it == g.textures.end()) return;
  FreeTexture(it->second);
  g.textures.erase(it);''', '''  Texture* it = g.textures.find(id);
  if (!it) return;
  FreeTexture(*it);
  g.textures.erase(id);''')
open(sys.argv[1] + '/ugui_impl_dx11_patched.cc', 'w').write(s)
PY
flags="$(pkg-config --cflags --libs dxvk-d3d11 dxvk-dxgi glfw3 freetype2 harfbuzz)"
libs="$d/libultragui.a $d/third_party/yoga/yoga/libyogacore.a -ldl -lpthread"
clang++ -std=c++20 -O1 -I. -I"$d/generated" -o "$d/rhi_scene_d11" aki/tools/rhi_scene.cc $libs $flags
clang++ -std=c++20 -O1 -I. -I"$d/generated" -o "$d/draw_data_scene_dx11" aki/tools/draw_data_scene_dx11.cc \
  "$d/ugui_impl_dx11_patched.cc" $libs $flags
