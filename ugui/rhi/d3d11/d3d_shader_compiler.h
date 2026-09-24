#ifndef ULTRAGUI_RHI_D3D11_D3D_SHADER_COMPILER_H_
#define ULTRAGUI_RHI_D3D11_D3D_SHADER_COMPILER_H_

#include <ugui/core/config.h>

namespace ugui {
namespace d3d {

/// Compiles HLSL to DXBC, which D3D11 consumes and dxc no longer emits.
///
/// The compiler is loaded by name at first use: d3dcompiler_47.dll on
/// Windows, vkd3d-utils on Linux (DXVK-native builds). Shared by the D3D11
/// RHI and the ugui_impl_dx11 draw-data backend.
///
/// `defines` is a null-terminated list of macro names, each defined as 1, or
/// null for none.
bool CompileHlsl(const char* source, const char* entry, const char* target,
                 Vector<char>& out, const char* const* defines = nullptr);

}  // namespace d3d
}  // namespace ugui

#endif  // ULTRAGUI_RHI_D3D11_D3D_SHADER_COMPILER_H_
