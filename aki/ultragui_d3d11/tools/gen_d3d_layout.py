#!/usr/bin/env python3
"""Layout reference for the D3D11/DXGI declarations in src/sys/d3d11*.aki.

  python3 tools/gen_d3d_layout.py cc  <dxvk include dir> > /tmp/d3d_layout.cc
  clang++ -std=c++17 -I<dxvk include dir> /tmp/d3d_layout.cc -o /tmp/d3d_layout && /tmp/d3d_layout > /tmp/d3d_layout.txt
  python3 tools/gen_d3d_layout.py aki /tmp/d3d_layout.txt > tests/support/d3d_layout_table.aki

The C++ prints, from the headers ultragui_d3d11 mirrors, every struct size,
field offset, constant and used vtable-slot offset; the aki side evaluates the
same names on its declarations (tests/d3d11_layout_test.aki compares).
"""
import re
import sys

# (aki type, C type, [(aki field, C field path)])
STRUCTS = [
    ("Guid", "GUID", ["data1", "data2", "data3", "data4"], ["Data1", "Data2", "Data3", "Data4"]),
    ("DxgiSampleDesc", "DXGI_SAMPLE_DESC", ["count", "quality"], ["Count", "Quality"]),
    ("DxgiRational", "DXGI_RATIONAL", ["numerator", "denominator"], ["Numerator", "Denominator"]),
    ("DxgiModeDesc", "DXGI_MODE_DESC",
     ["width", "height", "refresh_rate", "format", "scanline_ordering", "scaling"],
     ["Width", "Height", "RefreshRate", "Format", "ScanlineOrdering", "Scaling"]),
    ("DxgiSwapChainDesc", "DXGI_SWAP_CHAIN_DESC",
     ["buffer_desc", "sample_desc", "buffer_usage", "buffer_count", "output_window", "windowed", "swap_effect", "flags"],
     ["BufferDesc", "SampleDesc", "BufferUsage", "BufferCount", "OutputWindow", "Windowed", "SwapEffect", "Flags"]),
    ("BufferDesc", "D3D11_BUFFER_DESC",
     ["byte_width", "usage", "bind_flags", "cpu_access_flags", "misc_flags", "structure_byte_stride"],
     ["ByteWidth", "Usage", "BindFlags", "CPUAccessFlags", "MiscFlags", "StructureByteStride"]),
    ("Texture2dDesc", "D3D11_TEXTURE2D_DESC",
     ["width", "height", "mip_levels", "array_size", "format", "sample_desc", "usage", "bind_flags", "cpu_access_flags", "misc_flags"],
     ["Width", "Height", "MipLevels", "ArraySize", "Format", "SampleDesc", "Usage", "BindFlags", "CPUAccessFlags", "MiscFlags"]),
    ("SubresourceData", "D3D11_SUBRESOURCE_DATA", ["sys_mem", "sys_mem_pitch", "sys_mem_slice_pitch"],
     ["pSysMem", "SysMemPitch", "SysMemSlicePitch"]),
    ("MappedSubresource", "D3D11_MAPPED_SUBRESOURCE", ["data", "row_pitch", "depth_pitch"], ["pData", "RowPitch", "DepthPitch"]),
    ("ShaderResourceViewDesc", "D3D11_SHADER_RESOURCE_VIEW_DESC",
     ["format", "view_dimension", "most_detailed_mip", "mip_levels"],
     ["Format", "ViewDimension", "Texture2D.MostDetailedMip", "Texture2D.MipLevels"]),
    ("RenderTargetViewDesc", "D3D11_RENDER_TARGET_VIEW_DESC", ["format", "view_dimension", "mip_slice"],
     ["Format", "ViewDimension", "Texture2D.MipSlice"]),
    ("InputElementDesc", "D3D11_INPUT_ELEMENT_DESC",
     ["semantic_name", "semantic_index", "format", "input_slot", "aligned_byte_offset", "input_slot_class", "instance_data_step_rate"],
     ["SemanticName", "SemanticIndex", "Format", "InputSlot", "AlignedByteOffset", "InputSlotClass", "InstanceDataStepRate"]),
    ("RenderTargetBlendDesc", "D3D11_RENDER_TARGET_BLEND_DESC",
     ["blend_enable", "src_blend", "dest_blend", "blend_op", "src_blend_alpha", "dest_blend_alpha", "blend_op_alpha", "render_target_write_mask"],
     ["BlendEnable", "SrcBlend", "DestBlend", "BlendOp", "SrcBlendAlpha", "DestBlendAlpha", "BlendOpAlpha", "RenderTargetWriteMask"]),
    ("BlendDesc", "D3D11_BLEND_DESC", ["alpha_to_coverage_enable", "independent_blend_enable", "render_target"],
     ["AlphaToCoverageEnable", "IndependentBlendEnable", "RenderTarget"]),
    ("RasterizerDesc", "D3D11_RASTERIZER_DESC",
     ["fill_mode", "cull_mode", "front_counter_clockwise", "depth_bias", "depth_bias_clamp", "slope_scaled_depth_bias", "depth_clip_enable", "scissor_enable", "multisample_enable", "antialiased_line_enable"],
     ["FillMode", "CullMode", "FrontCounterClockwise", "DepthBias", "DepthBiasClamp", "SlopeScaledDepthBias", "DepthClipEnable", "ScissorEnable", "MultisampleEnable", "AntialiasedLineEnable"]),
    ("DepthStencilopDesc", "D3D11_DEPTH_STENCILOP_DESC", ["stencil_fail_op", "stencil_depth_fail_op", "stencil_pass_op", "stencil_func"],
     ["StencilFailOp", "StencilDepthFailOp", "StencilPassOp", "StencilFunc"]),
    ("DepthStencilDesc", "D3D11_DEPTH_STENCIL_DESC",
     ["depth_enable", "depth_write_mask", "depth_func", "stencil_enable", "stencil_read_mask", "stencil_write_mask", "front_face", "back_face"],
     ["DepthEnable", "DepthWriteMask", "DepthFunc", "StencilEnable", "StencilReadMask", "StencilWriteMask", "FrontFace", "BackFace"]),
    ("SamplerDesc", "D3D11_SAMPLER_DESC",
     ["filter", "address_u", "address_v", "address_w", "mip_lod_bias", "max_anisotropy", "comparison_func", "border_color", "min_lod", "max_lod"],
     ["Filter", "AddressU", "AddressV", "AddressW", "MipLODBias", "MaxAnisotropy", "ComparisonFunc", "BorderColor", "MinLOD", "MaxLOD"]),
    ("Viewport", "D3D11_VIEWPORT", ["top_left_x", "top_left_y", "width", "height", "min_depth", "max_depth"],
     ["TopLeftX", "TopLeftY", "Width", "Height", "MinDepth", "MaxDepth"]),
    ("D3dRect", "D3D11_RECT", ["left", "top", "right", "bottom"], ["left", "top", "right", "bottom"]),
]

# (aki constant, C expression)
CONSTS = [
    ("kD3dDriverTypeHardware", "D3D_DRIVER_TYPE_HARDWARE"), ("kD3dFeatureLevel11_0", "D3D_FEATURE_LEVEL_11_0"),
    ("kD3dFeatureLevel11_1", "D3D_FEATURE_LEVEL_11_1"), ("kD3d11SdkVersion", "D3D11_SDK_VERSION"),
    ("kDxgiFormatUnknown", "DXGI_FORMAT_UNKNOWN"), ("kDxgiFormatR32G32Float", "DXGI_FORMAT_R32G32_FLOAT"),
    ("kDxgiFormatR32Float", "DXGI_FORMAT_R32_FLOAT"), ("kDxgiFormatR32Uint", "DXGI_FORMAT_R32_UINT"),
    ("kDxgiFormatR8G8B8A8Typeless", "DXGI_FORMAT_R8G8B8A8_TYPELESS"), ("kDxgiFormatR8G8B8A8Unorm", "DXGI_FORMAT_R8G8B8A8_UNORM"),
    ("kDxgiFormatR8G8B8A8UnormSrgb", "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB"), ("kDxgiFormatR8Unorm", "DXGI_FORMAT_R8_UNORM"),
    ("kDxgiFormatB8G8R8A8Unorm", "DXGI_FORMAT_B8G8R8A8_UNORM"), ("kDxgiFormatB8G8R8X8Unorm", "DXGI_FORMAT_B8G8R8X8_UNORM"),
    ("kDxgiFormatB8G8R8A8UnormSrgb", "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB"), ("kDxgiFormatB8G8R8X8UnormSrgb", "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB"),
    ("kDxgiUsageRenderTargetOutput", "DXGI_USAGE_RENDER_TARGET_OUTPUT"), ("kDxgiSwapEffectDiscard", "DXGI_SWAP_EFFECT_DISCARD"),
    ("kUsageDefault", "D3D11_USAGE_DEFAULT"), ("kUsageDynamic", "D3D11_USAGE_DYNAMIC"),
    ("kBindVertexBuffer", "D3D11_BIND_VERTEX_BUFFER"), ("kBindIndexBuffer", "D3D11_BIND_INDEX_BUFFER"),
    ("kBindConstantBuffer", "D3D11_BIND_CONSTANT_BUFFER"), ("kBindShaderResource", "D3D11_BIND_SHADER_RESOURCE"),
    ("kBindRenderTarget", "D3D11_BIND_RENDER_TARGET"), ("kCpuAccessWrite", "D3D11_CPU_ACCESS_WRITE"),
    ("kMapWriteDiscard", "D3D11_MAP_WRITE_DISCARD"), ("kMapWriteNoOverwrite", "D3D11_MAP_WRITE_NO_OVERWRITE"),
    ("kInputPerVertexData", "D3D11_INPUT_PER_VERTEX_DATA"), ("kBlendOne", "D3D11_BLEND_ONE"),
    ("kBlendSrcAlpha", "D3D11_BLEND_SRC_ALPHA"), ("kBlendInvSrcAlpha", "D3D11_BLEND_INV_SRC_ALPHA"),
    ("kBlendOpAdd", "D3D11_BLEND_OP_ADD"), ("kColorWriteEnableAll", "D3D11_COLOR_WRITE_ENABLE_ALL"),
    ("kFillSolid", "D3D11_FILL_SOLID"), ("kCullNone", "D3D11_CULL_NONE"), ("kDepthWriteMaskAll", "D3D11_DEPTH_WRITE_MASK_ALL"),
    ("kComparisonNever", "D3D11_COMPARISON_NEVER"), ("kComparisonAlways", "D3D11_COMPARISON_ALWAYS"),
    ("kStencilOpKeep", "D3D11_STENCIL_OP_KEEP"), ("kFilterMinMagMipPoint", "D3D11_FILTER_MIN_MAG_MIP_POINT"),
    ("kFilterMinMagMipLinear", "D3D11_FILTER_MIN_MAG_MIP_LINEAR"), ("kTextureAddressClamp", "D3D11_TEXTURE_ADDRESS_CLAMP"),
    ("kSrvDimensionTexture2d", "D3D11_SRV_DIMENSION_TEXTURE2D"), ("kRtvDimensionTexture2d", "D3D11_RTV_DIMENSION_TEXTURE2D"),
    ("kPrimitiveTopologyUndefined", "D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED"),
    ("kPrimitiveTopologyTriangleList", "D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST"),
    ("kViewportAndScissorrectObjectCountPerPipeline", "D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE"),
]

# aki vtable struct, C vtable type, header
VTBLS = [("DeviceVtbl", "ID3D11DeviceVtbl"), ("DeviceContextVtbl", "ID3D11DeviceContextVtbl"),
         ("RenderTargetViewVtbl", "ID3D11RenderTargetViewVtbl"), ("SwapChainVtbl", "IDXGISwapChainVtbl"),
         ("DxgiDeviceVtbl", "IDXGIDeviceVtbl"), ("DxgiAdapterVtbl", "IDXGIAdapterVtbl"), ("DxgiFactoryVtbl", "IDXGIFactoryVtbl")]


def snake(name):
    name = name.replace("2D", "2d")
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])", "_", name).lower()


def used_slots(vt):
    """(aki field, C name) for the typed slots of an aki vtable struct, from the generated module."""
    src = open("src/sys/d3d11_vtbl.aki").read()
    m = re.search(r"pub extern struct %s \{(.*?)\n\}" % vt, src, re.S)
    fields = re.findall(r"pub var (\w+): extern fn", m.group(1))
    return fields


def c_slot_name(vt_c, aki_field, hdr_dir):
    for h in ("d3d11.h", "dxgi.h"):
        s = open("%s/%s" % (hdr_dir, h)).read()
        m = re.search(r"typedef struct %s \{(.*?)\} %s;" % (vt_c, vt_c), s, re.S)
        if m:
            for n in re.findall(r"\(STDMETHODCALLTYPE \*(\w+)\)", m.group(1)):
                if snake(n) == aki_field:
                    return n
    raise SystemExit("no C slot for %s.%s" % (vt_c, aki_field))


def main():
    mode = sys.argv[1]
    if mode == "cc":
        hdr = sys.argv[2]
        print("// Generated by tools/gen_d3d_layout.py: do not edit.")
        print("#define CINTERFACE\n#define COBJMACROS\n#define D3D11_NO_HELPERS\n#include <d3d11.h>\n#include <dxgi.h>\n#include <stddef.h>\n#include <stdio.h>")
        print("#pragma clang diagnostic ignored \"-Winvalid-offsetof\"")
        print("int main() {")
        for a, c, afields, cfields in STRUCTS:
            print('  printf("SIZE %s %%zu\\n", sizeof(%s));' % (a, c))
            for af, cf in zip(afields, cfields):
                print('  printf("OFFSET %s.%s %%zu\\n", offsetof(%s, %s));' % (a, af, c, cf))
        for a, c in CONSTS:
            print('  printf("CONST %s %%llu\\n", static_cast<unsigned long long>(%s));' % (a, c))
        for avt, cvt in VTBLS:
            print('  printf("SIZE %s %%zu\\n", sizeof(%s));' % (avt, cvt))
            for f in used_slots(avt):
                cn = c_slot_name(cvt, f, hdr)
                print('  printf("OFFSET %s.%s %%zu\\n", offsetof(%s, %s));' % (avt, f, cvt, cn))
        print("  return 0;\n}")
    else:
        vals = {}
        for line in open(sys.argv[2]):
            kind, name, v = line.split()
            vals[(kind, name)] = int(v)
        print("// Generated by tools/gen_d3d_layout.py from the C headers: do not edit.")
        print("// Size, offset and constant references for tests/d3d11_layout_test.aki.")
        print("")
        print("module support::d3d_layout_table;")
        print("")
        print("import {List} from akikit::containers::list;")
        print("import ultragui_d3d11::sys::d3d11;")
        print("import ultragui_d3d11::sys::d3d11_vtbl;")
        print("")
        print("// One reference: its name, the aki value, the C header's.")
        print("pub struct Entry : Copy {")
        print("  pub let name: str")
        print("  pub let got: u64")
        print("  pub let want: u64")
        print("}")
        print("")
        print("pub fn entries(): List<Entry> {")
        print("  var e: List<Entry> = List();")
        for a, c, afields, cfields in STRUCTS:
            print('  e.push(Entry { name: "size %s", got: @size_of(ultragui_d3d11::sys::d3d11::%s) as u64, want: %d });' % (a, a, vals[("SIZE", a)]))
            for af in afields:
                print('  e.push(Entry { name: "offset %s.%s", got: @offset_of(ultragui_d3d11::sys::d3d11::%s, %s) as u64, want: %d });' % (a, af, a, af, vals[("OFFSET", a + "." + af)]))
        for a, c in CONSTS:
            print('  e.push(Entry { name: "%s", got: ultragui_d3d11::sys::d3d11::%s as u64, want: %d });' % (a, a, vals[("CONST", a)]))
        for avt, cvt in VTBLS:
            print('  e.push(Entry { name: "size %s", got: @size_of(ultragui_d3d11::sys::d3d11_vtbl::%s) as u64, want: %d });' % (avt, avt, vals[("SIZE", avt)]))
            for f in used_slots(avt):
                print('  e.push(Entry { name: "offset %s.%s", got: @offset_of(ultragui_d3d11::sys::d3d11_vtbl::%s, %s) as u64, want: %d });' % (avt, f, avt, f, vals[("OFFSET", avt + "." + f)]))
        print("  return e;")
        print("}")


main()
