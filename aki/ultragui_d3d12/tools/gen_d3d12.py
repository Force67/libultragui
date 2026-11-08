#!/usr/bin/env python3
"""Generates the COM section of src/sys/d3d12.aki and checks the hand-written
structs and constants of that file against vkd3d's headers.

  python3 -I tools/gen_d3d12.py <vkd3d include dir> com        # rewrite the generated section
  python3 -I tools/gen_d3d12.py <vkd3d include dir> layout     # write tools/layout.c and tests/support/layout_ref.aki

The include dir is the one pkg-config reports for libvkd3d (inside the dev shell:
`pkg-config --cflags libvkd3d` -> -I<dir>/vkd3d). `layout` also compiles and runs
tools/layout.c against vkd3d and the Vulkan headers (clang from PATH).
"""
import os
import re
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
pkg = os.path.dirname(here)
inc = sys.argv[1]
mode = sys.argv[2]

hdr_d3d12 = open(os.path.join(inc, "vkd3d_d3d12.h")).read()
hdr_common = open(os.path.join(inc, "vkd3d_d3dcommon.h")).read()
hdrs = hdr_d3d12 + "\n" + hdr_common


def snake(s):
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])", "_", s).lower()


# (interface, wrapper prefix, methods the port calls). The order of the vtable
# comes from the header; inherited interfaces are part of it.
INTERFACES = [
    ("ID3D12Device", "dev", [
        "CreateCommandQueue", "CreateCommandAllocator", "CreateGraphicsPipelineState", "CreateCommandList",
        "CreateDescriptorHeap", "GetDescriptorHandleIncrementSize", "CreateRootSignature",
        "CreateShaderResourceView", "CreateRenderTargetView", "CreateCommittedResource", "CreateFence",
        "GetCopyableFootprints"]),
    ("ID3D12CommandQueue", "queue", ["ExecuteCommandLists", "Signal"]),
    ("ID3D12CommandAllocator", "alloc", ["Reset"]),
    ("ID3D12CommandList", "cmd_list", []),
    ("ID3D12GraphicsCommandList", "cl", [
        "Close", "Reset", "DrawInstanced", "DrawIndexedInstanced", "CopyTextureRegion", "IASetPrimitiveTopology",
        "RSSetViewports", "RSSetScissorRects", "SetPipelineState", "ResourceBarrier", "SetDescriptorHeaps",
        "SetGraphicsRootSignature", "SetGraphicsRootDescriptorTable", "SetGraphicsRoot32BitConstants",
        "IASetIndexBuffer", "IASetVertexBuffers", "OMSetRenderTargets", "ClearRenderTargetView"]),
    ("ID3D12Fence", "fence", ["GetCompletedValue", "Signal"]),
    ("ID3D12DescriptorHeap", "heap", ["GetCPUDescriptorHandleForHeapStart", "GetGPUDescriptorHandleForHeapStart"]),
    ("ID3D12Resource", "res", ["Map", "Unmap", "GetDesc", "GetGPUVirtualAddress"]),
    ("ID3D12RootSignature", "root_sig", []),
    ("ID3D12PipelineState", "pso", []),
    ("ID3D10Blob", "blob", ["GetBufferPointer", "GetBufferSize"]),
]
IUNKNOWN = "IUnknown"
IIDS = ["ID3D12Device", "ID3D12CommandQueue", "ID3D12CommandAllocator", "ID3D12GraphicsCommandList", "ID3D12Fence",
        "ID3D12DescriptorHeap", "ID3D12Resource", "ID3D12RootSignature", "ID3D12PipelineState"]
IFACE_NAMES = {i[0] for i in INTERFACES} | {"IUnknown"}

# C type -> aki type. Structs the file defines are looked up by their C name.
STRUCTS = {
    "D3D12_COMMAND_QUEUE_DESC": "CommandQueueDesc", "D3D12_GRAPHICS_PIPELINE_STATE_DESC": "GraphicsPipelineStateDesc",
    "D3D12_DESCRIPTOR_HEAP_DESC": "DescriptorHeapDesc", "D3D12_SHADER_RESOURCE_VIEW_DESC": "ShaderResourceViewDesc",
    "D3D12_RENDER_TARGET_VIEW_DESC": "RenderTargetViewDesc", "D3D12_HEAP_PROPERTIES": "HeapProperties",
    "D3D12_RESOURCE_DESC": "ResourceDesc", "D3D12_CLEAR_VALUE": "ClearValue",
    "D3D12_PLACED_SUBRESOURCE_FOOTPRINT": "PlacedSubresourceFootprint", "D3D12_RESOURCE_BARRIER": "ResourceBarrier",
    "D3D12_TEXTURE_COPY_LOCATION": "TextureCopyLocation", "D3D12_VIEWPORT": "Viewport", "D3D12_RECT": "D3dRect",
    "D3D12_VERTEX_BUFFER_VIEW": "VertexBufferView", "D3D12_INDEX_BUFFER_VIEW": "IndexBufferView",
    "D3D12_CPU_DESCRIPTOR_HANDLE": "CpuDescriptorHandle", "D3D12_GPU_DESCRIPTOR_HANDLE": "GpuDescriptorHandle",
    "D3D12_RANGE": "Range", "D3D12_BOX": "D3dBox",
}
SCALARS = {
    "HRESULT": "i32", "ULONG": "u32", "UINT": "u32", "UINT64": "u64", "UINT8": "u8", "UINT16": "u16", "SIZE_T": "usize",
    "INT": "i32", "FLOAT": "f32", "BOOL": "i32", "D3D12_GPU_VIRTUAL_ADDRESS": "u64", "LUID": "Luid",
    "D3D12_COMMAND_LIST_TYPE": "u32", "DXGI_FORMAT": "u32", "D3D12_RESOURCE_STATES": "u32",
    "D3D12_HEAP_FLAGS": "u32", "D3D12_DESCRIPTOR_HEAP_TYPE": "u32", "D3D_PRIMITIVE_TOPOLOGY": "u32",
    "D3D12_DESCRIPTOR_HEAP_FLAGS": "u32", "D3D12_PRIMITIVE_TOPOLOGY": "u32", "D3D12_PRIMITIVE_TOPOLOGY_TYPE": "u32",
}
KEYWORDS = {"type", "fn", "let", "var", "in", "loop", "match", "null", "ref", "mod", "self", "struct", "enum", "extern",
            "end", "start", "range"}


def aki_type(c):
    c = re.sub(r"\s+", " ", c.strip())
    if c in ("REFIID", "REFGUID"):
        return "*const Guid"
    special = {"ID3D12CommandList *const *": "*const *var ID3D12CommandList",
               "ID3D12DescriptorHeap *const *": "*const *var ID3D12DescriptorHeap",
               "void * *": "*var *var u8", "void **": "*var *var u8"}
    if c in special:
        return special[c]
    m = re.fullmatch(r"(const )?(.+?) ?\*( ?const)? ?(\*)?", c)
    if c == "void":
        return "()"
    if c in SCALARS:
        return SCALARS[c]
    if c in STRUCTS:
        return STRUCTS[c]
    if c.startswith("D3D12_") or c.startswith("D3D_") or c.startswith("DXGI_"):
        if c.endswith("_FLAGS") or c.endswith("_TYPE") or c.endswith("_STATES") or c.endswith("_MODE") or c.endswith("_FORMAT"):
            return "u32"
    # pointer forms
    if c.endswith("*"):
        inner = c[:-1].strip()
        if inner.endswith("const"):  # X *const *
            pass
        const = inner.startswith("const ")
        if const:
            inner = inner[6:].strip()
        if inner == "void":
            return "*const u8" if const else "*var u8"
        if inner == "void *":
            return "*var *var u8"
        if inner == "ID3D12CommandList *const":
            return "*const *var ID3D12CommandList"
        if inner == "ID3D12DescriptorHeap *":
            return "*const *var ID3D12DescriptorHeap"
        if inner in IFACE_NAMES:
            return "*var " + inner
        if inner in STRUCTS:
            return ("*const " if const else "*var ") + STRUCTS[inner]
        if inner in SCALARS:
            return ("*const " if const else "*var ") + SCALARS[inner]
    raise KeyError(c)


def parse_vtbl(name):
    m = re.search(r"typedef struct %sVtbl \{(.*?)\} %sVtbl;" % (name, name), hdrs, re.S)
    body = m.group(1)
    out = []
    for mm in re.finditer(r"([^\n;(]+?)\s*\(STDMETHODCALLTYPE \*(\w+)\)\(\s*(.*?)\);", body, re.S):
        ret, meth, params = mm.group(1).strip(), mm.group(2), mm.group(3)
        plist = []
        for p in re.split(r",\s*", params.strip()):
            p = p.strip()
            if not p:
                continue
            p = re.sub(r"(\w+)\[\d+\]$", r"* \1", p)  # `const FLOAT color[4]` is a pointer parameter
            pm = re.fullmatch(r"(.+?)\s*(\*?)(\w+)", p)
            ptype = (pm.group(1) + (" *" if pm.group(2) else "")).strip()
            plist.append((ptype, pm.group(3)))
        out.append((meth, ret, plist))
    return out


def gen_com():
    out = []
    w = out.append
    w("// IIDs (the values <vkd3d_d3d12.h> defines with INITGUID in the C++).")
    for iid in IIDS + ["ID3D10Blob"]:
        mm = re.search(r"DEFINE_GUID\(IID_%s,\s*(0x\w+),\s*(0x\w+),\s*(0x\w+),((?:\s*0x\w+,?){8})\);" % iid, hdrs)
        d4 = [x for x in re.findall(r"0x\w+", mm.group(4))]
        w("pub static let kIid%s: Guid = Guid { data1: %s, data2: %s, data3: %s, data4: [%s] };" % (
            iid[1:] if iid.startswith("ID3D12") is False else iid[6:], mm.group(1), mm.group(2), mm.group(3), ", ".join(d4)))
    w("")
    # IUnknown
    w("pub extern struct IUnknown { pub var vtbl: *const IUnknownVtbl = null }")
    w("")
    w("pub extern struct IUnknownVtbl {")
    w("  pub var query_interface: extern fn(*var IUnknown, *const Guid, *var *var u8): Hresult")
    w("  pub var add_ref: extern fn(*var IUnknown): u32")
    w("  pub var release: extern fn(*var IUnknown): u32")
    w("}")
    w("")
    w("// IUnknown::Release on any COM object (the C++ ID3D12*_Release macros); returns the new count.")
    w("pub unsafe fn com_release<T>(object: *var T): u32 => unsafe ((*(*(object as *var IUnknown)).vtbl).release)(object as *var IUnknown);")
    w("")
    wrappers = []
    for iface, prefix, used in INTERFACES:
        meths = parse_vtbl(iface)
        w("pub extern struct %s { pub var vtbl: *const %sVtbl = null }" % (iface, iface))
        w("")
        w("pub extern struct %sVtbl {" % iface)
        for meth, ret, plist in meths:
            sn = snake(meth)
            if meth in used or meth in ("QueryInterface", "AddRef", "Release"):
                ptypes = []
                for ptype, pname in plist:
                    if pname == "This":
                        ptypes.append("*var " + iface)
                    else:
                        try:
                            ptypes.append(aki_type(ptype))
                        except KeyError:
                            if meth in used:
                                raise SystemExit("no aki type for %s in %s::%s (%s)" % (ptype, iface, meth, pname))
                            ptypes.append("*const u8")
                hidden = plist and plist[-1][1] == "__ret"
                rt = "*var " + aki_type(plist[-1][0][:-2].strip()) if False else None
                if hidden:
                    rtype = "*var " + aki_type(plist[-1][0].rstrip("* ").strip())
                    ptypes[-1] = rtype
                else:
                    rtype = aki_type(ret)
                w("  pub var %s: extern fn(%s): %s" % (sn, ", ".join(ptypes), rtype))
                if meth in used:
                    # wrapper
                    names = []
                    ptypes2 = []
                    for (ptype, pname), at in zip(plist, ptypes):
                        if pname == "This":
                            names.append("this")
                            ptypes2.append("this: " + at)
                        elif pname == "__ret":
                            continue
                        else:
                            n = pname + "_" if pname in KEYWORDS else pname
                            names.append(n)
                            ptypes2.append("%s: %s" % (n, at))
                    if hidden:
                        rt_s = at_ret = ptypes[-1][len("*var "):]
                        wrappers.append(
                            "pub unsafe fn %s_%s(%s): %s {\n  var ret = %s {};\n  unsafe { (*(*this).vtbl).%s(%s, &raw var ret); }\n  return ret;\n}" % (
                                prefix, sn, ", ".join(ptypes2), at_ret, at_ret, sn, ", ".join(names)))
                    else:
                        rt_s = aki_type(ret)
                        sig_ret = "" if rt_s == "()" else ": " + rt_s
                        wrappers.append("pub unsafe fn %s_%s(%s)%s => unsafe (*(*this).vtbl).%s(%s);" % (
                            prefix, sn, ", ".join(ptypes2), sig_ret, sn, ", ".join(names))
                                        if rt_s != "()" else
                                        "pub unsafe fn %s_%s(%s) {\n  unsafe { (*(*this).vtbl).%s(%s); }\n}" % (
                                            prefix, sn, ", ".join(ptypes2), sn, ", ".join(names)))
            else:
                w("  pub var %s: *const u8 = null" % sn)
        w("}")
        w("")
    w("// One wrapper per method the port calls: `<iface>_<method>(this, ...)`.")
    for x in wrappers:
        w(x)
        w("")
    return "\n".join(out)


# --- layout / constants check -------------------------------------------------

# (aki struct, C struct, C field names in the order of the aki fields)
LAYOUT = [
    ("Guid", "GUID", ["Data1", "Data2", "Data3", "Data4"]),
    ("Luid", "LUID", ["LowPart", "HighPart"]),
    ("D3dRect", "D3D12_RECT", ["left", "top", "right", "bottom"]),
    ("D3dBox", "D3D12_BOX", ["left", "top", "front", "right", "bottom", "back"]),
    ("Range", "D3D12_RANGE", ["Begin", "End"]),
    ("HeapProperties", "D3D12_HEAP_PROPERTIES",
     ["Type", "CPUPageProperty", "MemoryPoolPreference", "CreationNodeMask", "VisibleNodeMask"]),
    ("SampleDesc", "DXGI_SAMPLE_DESC", ["Count", "Quality"]),
    ("ResourceDesc", "D3D12_RESOURCE_DESC",
     ["Dimension", "Alignment", "Width", "Height", "DepthOrArraySize", "MipLevels", "Format", "SampleDesc", "Layout", "Flags"]),
    ("DepthStencilValue", "D3D12_DEPTH_STENCIL_VALUE", ["Depth", "Stencil"]),
    ("ClearValueData", "D3D12_CLEAR_VALUE_DATA_UNUSED", None),
    ("ClearValue", "D3D12_CLEAR_VALUE", ["Format", "Color"]),
    ("DescriptorRange", "D3D12_DESCRIPTOR_RANGE",
     ["RangeType", "NumDescriptors", "BaseShaderRegister", "RegisterSpace", "OffsetInDescriptorsFromTableStart"]),
    ("RootDescriptorTable", "D3D12_ROOT_DESCRIPTOR_TABLE", ["NumDescriptorRanges", "pDescriptorRanges"]),
    ("RootConstants", "D3D12_ROOT_CONSTANTS", ["ShaderRegister", "RegisterSpace", "Num32BitValues"]),
    ("RootDescriptor", "D3D12_ROOT_DESCRIPTOR", ["ShaderRegister", "RegisterSpace"]),
    ("RootParameter", "D3D12_ROOT_PARAMETER", ["ParameterType", "DescriptorTable", "ShaderVisibility"]),
    ("StaticSamplerDesc", "D3D12_STATIC_SAMPLER_DESC",
     ["Filter", "AddressU", "AddressV", "AddressW", "MipLODBias", "MaxAnisotropy", "ComparisonFunc", "BorderColor",
      "MinLOD", "MaxLOD", "ShaderRegister", "RegisterSpace", "ShaderVisibility"]),
    ("RootSignatureDesc", "D3D12_ROOT_SIGNATURE_DESC",
     ["NumParameters", "pParameters", "NumStaticSamplers", "pStaticSamplers", "Flags"]),
    ("InputElementDesc", "D3D12_INPUT_ELEMENT_DESC",
     ["SemanticName", "SemanticIndex", "Format", "InputSlot", "AlignedByteOffset", "InputSlotClass", "InstanceDataStepRate"]),
    ("ShaderBytecode", "D3D12_SHADER_BYTECODE", ["pShaderBytecode", "BytecodeLength"]),
    ("StreamOutputDesc", "D3D12_STREAM_OUTPUT_DESC",
     ["pSODeclaration", "NumEntries", "pBufferStrides", "NumStrides", "RasterizedStream"]),
    ("RenderTargetBlendDesc", "D3D12_RENDER_TARGET_BLEND_DESC",
     ["BlendEnable", "LogicOpEnable", "SrcBlend", "DestBlend", "BlendOp", "SrcBlendAlpha", "DestBlendAlpha",
      "BlendOpAlpha", "LogicOp", "RenderTargetWriteMask"]),
    ("BlendDesc", "D3D12_BLEND_DESC", ["AlphaToCoverageEnable", "IndependentBlendEnable", "RenderTarget"]),
    ("RasterizerDesc", "D3D12_RASTERIZER_DESC",
     ["FillMode", "CullMode", "FrontCounterClockwise", "DepthBias", "DepthBiasClamp", "SlopeScaledDepthBias",
      "DepthClipEnable", "MultisampleEnable", "AntialiasedLineEnable", "ForcedSampleCount", "ConservativeRaster"]),
    ("DepthStencilopDesc", "D3D12_DEPTH_STENCILOP_DESC",
     ["StencilFailOp", "StencilDepthFailOp", "StencilPassOp", "StencilFunc"]),
    ("DepthStencilDesc", "D3D12_DEPTH_STENCIL_DESC",
     ["DepthEnable", "DepthWriteMask", "DepthFunc", "StencilEnable", "StencilReadMask", "StencilWriteMask", "FrontFace",
      "BackFace"]),
    ("InputLayoutDesc", "D3D12_INPUT_LAYOUT_DESC", ["pInputElementDescs", "NumElements"]),
    ("CachedPipelineState", "D3D12_CACHED_PIPELINE_STATE", ["pCachedBlob", "CachedBlobSizeInBytes"]),
    ("GraphicsPipelineStateDesc", "D3D12_GRAPHICS_PIPELINE_STATE_DESC",
     ["pRootSignature", "VS", "PS", "DS", "HS", "GS", "StreamOutput", "BlendState", "SampleMask", "RasterizerState",
      "DepthStencilState", "InputLayout", "IBStripCutValue", "PrimitiveTopologyType", "NumRenderTargets", "RTVFormats",
      "DSVFormat", "SampleDesc", "NodeMask", "CachedPSO", "Flags"]),
    ("ResourceTransitionBarrier", "D3D12_RESOURCE_TRANSITION_BARRIER",
     ["pResource", "Subresource", "StateBefore", "StateAfter"]),
    ("ResourceAliasingBarrier", "D3D12_RESOURCE_ALIASING_BARRIER", ["pResourceBefore", "pResourceAfter"]),
    ("ResourceUavBarrier", "D3D12_RESOURCE_UAV_BARRIER", ["pResource"]),
    ("ResourceBarrier", "D3D12_RESOURCE_BARRIER", ["Type", "Flags", "Transition"]),
    ("VertexBufferView", "D3D12_VERTEX_BUFFER_VIEW", ["BufferLocation", "SizeInBytes", "StrideInBytes"]),
    ("IndexBufferView", "D3D12_INDEX_BUFFER_VIEW", ["BufferLocation", "SizeInBytes", "Format"]),
    ("Viewport", "D3D12_VIEWPORT", ["TopLeftX", "TopLeftY", "Width", "Height", "MinDepth", "MaxDepth"]),
    ("Tex2dSrv", "D3D12_TEX2D_SRV", ["MostDetailedMip", "MipLevels", "PlaneSlice", "ResourceMinLODClamp"]),
    ("ShaderResourceViewDesc", "D3D12_SHADER_RESOURCE_VIEW_DESC",
     ["Format", "ViewDimension", "Shader4ComponentMapping", "Texture2D"]),
    ("Tex2dRtv", "D3D12_TEX2D_RTV", ["MipSlice", "PlaneSlice"]),
    ("RenderTargetViewDesc", "D3D12_RENDER_TARGET_VIEW_DESC", ["Format", "ViewDimension", "Texture2D"]),
    ("CommandQueueDesc", "D3D12_COMMAND_QUEUE_DESC", ["Type", "Priority", "Flags", "NodeMask"]),
    ("DescriptorHeapDesc", "D3D12_DESCRIPTOR_HEAP_DESC", ["Type", "NumDescriptors", "Flags", "NodeMask"]),
    ("SubresourceFootprint", "D3D12_SUBRESOURCE_FOOTPRINT", ["Format", "Width", "Height", "Depth", "RowPitch"]),
    ("PlacedSubresourceFootprint", "D3D12_PLACED_SUBRESOURCE_FOOTPRINT", ["Offset", "Footprint"]),
    ("TextureCopyLocation", "D3D12_TEXTURE_COPY_LOCATION", ["pResource", "Type", "PlacedFootprint"]),
    ("CpuDescriptorHandle", "D3D12_CPU_DESCRIPTOR_HANDLE", ["ptr"]),
    ("GpuDescriptorHandle", "D3D12_GPU_DESCRIPTOR_HANDLE", ["ptr"]),
    ("Vkd3dInstanceCreateInfo", "struct vkd3d_instance_create_info",
     ["type", "next", "pfn_signal_event", "pfn_create_thread", "pfn_join_thread", "wchar_size",
      "pfn_vkGetInstanceProcAddr", "instance_extensions", "instance_extension_count"]),
    ("Vkd3dDeviceCreateInfo", "struct vkd3d_device_create_info",
     ["type", "next", "minimum_feature_level", "instance", "instance_create_info", "vk_physical_device",
      "device_extensions", "device_extension_count", "parent", "adapter_luid"]),
    ("Vkd3dImageResourceCreateInfo", "struct vkd3d_image_resource_create_info",
     ["type", "next", "vk_image", "desc", "flags", "present_state"]),
]
# Structs whose aki field list is not the C one (the unions are named separately).
UNION_LAYOUT = [
    ("ClearValueData", "D3D12_CLEAR_VALUE", "Color"),
]

# (aki constant, C expression)
CONSTS = [
    ("kFeatureLevel11_0", "D3D_FEATURE_LEVEL_11_0"), ("kDxgiFormatUnknown", "DXGI_FORMAT_UNKNOWN"),
    ("kDxgiFormatR32G32Float", "DXGI_FORMAT_R32G32_FLOAT"), ("kDxgiFormatR8G8B8A8Unorm", "DXGI_FORMAT_R8G8B8A8_UNORM"),
    ("kDxgiFormatR8G8B8A8UnormSrgb", "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB"), ("kDxgiFormatR32Float", "DXGI_FORMAT_R32_FLOAT"),
    ("kDxgiFormatR32Uint", "DXGI_FORMAT_R32_UINT"), ("kDxgiFormatR8Unorm", "DXGI_FORMAT_R8_UNORM"),
    ("kDxgiFormatB8G8R8A8Unorm", "DXGI_FORMAT_B8G8R8A8_UNORM"),
    ("kDxgiFormatB8G8R8A8UnormSrgb", "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB"),
    ("kHeapTypeDefault", "D3D12_HEAP_TYPE_DEFAULT"), ("kHeapTypeUpload", "D3D12_HEAP_TYPE_UPLOAD"),
    ("kHeapFlagNone", "D3D12_HEAP_FLAG_NONE"),
    ("kResourceDimensionBuffer", "D3D12_RESOURCE_DIMENSION_BUFFER"),
    ("kResourceDimensionTexture2d", "D3D12_RESOURCE_DIMENSION_TEXTURE2D"),
    ("kTextureLayoutUnknown", "D3D12_TEXTURE_LAYOUT_UNKNOWN"), ("kTextureLayoutRowMajor", "D3D12_TEXTURE_LAYOUT_ROW_MAJOR"),
    ("kResourceFlagNone", "D3D12_RESOURCE_FLAG_NONE"),
    ("kResourceFlagAllowRenderTarget", "D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET"),
    ("kResourceStatePresent", "D3D12_RESOURCE_STATE_PRESENT"), ("kResourceStateRenderTarget", "D3D12_RESOURCE_STATE_RENDER_TARGET"),
    ("kResourceStateCopyDest", "D3D12_RESOURCE_STATE_COPY_DEST"),
    ("kResourceStatePixelShaderResource", "D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE"),
    ("kResourceStateGenericRead", "D3D12_RESOURCE_STATE_GENERIC_READ"),
    ("kResourceBarrierTypeTransition", "D3D12_RESOURCE_BARRIER_TYPE_TRANSITION"),
    ("kResourceBarrierAllSubresources", "D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES"),
    ("kTextureCopyTypeSubresourceIndex", "D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX"),
    ("kTextureCopyTypePlacedFootprint", "D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT"),
    ("kCommandListTypeDirect", "D3D12_COMMAND_LIST_TYPE_DIRECT"),
    ("kCommandQueuePriorityNormal", "D3D12_COMMAND_QUEUE_PRIORITY_NORMAL"),
    ("kCommandQueueFlagNone", "D3D12_COMMAND_QUEUE_FLAG_NONE"), ("kFenceFlagNone", "D3D12_FENCE_FLAG_NONE"),
    ("kDescriptorHeapTypeCbvSrvUav", "D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV"),
    ("kDescriptorHeapTypeRtv", "D3D12_DESCRIPTOR_HEAP_TYPE_RTV"),
    ("kDescriptorHeapFlagNone", "D3D12_DESCRIPTOR_HEAP_FLAG_NONE"),
    ("kDescriptorHeapFlagShaderVisible", "D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE"),
    ("kRootParameterTypeDescriptorTable", "D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE"),
    ("kRootParameterType32BitConstants", "D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS"),
    ("kShaderVisibilityVertex", "D3D12_SHADER_VISIBILITY_VERTEX"), ("kShaderVisibilityPixel", "D3D12_SHADER_VISIBILITY_PIXEL"),
    ("kDescriptorRangeTypeSrv", "D3D12_DESCRIPTOR_RANGE_TYPE_SRV"),
    ("kDescriptorRangeOffsetAppend", "D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND"),
    ("kRootSignatureFlagNone", "D3D12_ROOT_SIGNATURE_FLAG_NONE"),
    ("kRootSignatureFlagAllowInputAssemblerInputLayout", "D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT"),
    ("kRootSignatureVersion1", "D3D_ROOT_SIGNATURE_VERSION_1"),
    ("kFilterMinMagMipPoint", "D3D12_FILTER_MIN_MAG_MIP_POINT"), ("kFilterMinMagMipLinear", "D3D12_FILTER_MIN_MAG_MIP_LINEAR"),
    ("kTextureAddressModeClamp", "D3D12_TEXTURE_ADDRESS_MODE_CLAMP"),
    ("kBlendSrcAlpha", "D3D12_BLEND_SRC_ALPHA"), ("kBlendInvSrcAlpha", "D3D12_BLEND_INV_SRC_ALPHA"),
    ("kBlendOpAdd", "D3D12_BLEND_OP_ADD"), ("kColorWriteEnableAll", "D3D12_COLOR_WRITE_ENABLE_ALL"),
    ("kFillModeSolid", "D3D12_FILL_MODE_SOLID"), ("kCullModeNone", "D3D12_CULL_MODE_NONE"),
    ("kInputClassificationPerVertexData", "D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA"),
    ("kPrimitiveTopologyTypeTriangle", "D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE"),
    ("kPrimitiveTopologyTriangleList", "D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST"),
    ("kSrvDimensionTexture2d", "D3D12_SRV_DIMENSION_TEXTURE2D"), ("kRtvDimensionTexture2d", "D3D12_RTV_DIMENSION_TEXTURE2D"),
    ("kDefaultShader4ComponentMapping", "D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING"),
    ("kVkd3dStructureTypeInstanceCreateInfo", "VKD3D_STRUCTURE_TYPE_INSTANCE_CREATE_INFO"),
    ("kVkd3dStructureTypeDeviceCreateInfo", "VKD3D_STRUCTURE_TYPE_DEVICE_CREATE_INFO"),
    ("kVkd3dStructureTypeImageResourceCreateInfo", "VKD3D_STRUCTURE_TYPE_IMAGE_RESOURCE_CREATE_INFO"),
    ("kVkd3dResourceInitialStateTransition", "VKD3D_RESOURCE_INITIAL_STATE_TRANSITION"),
    ("kVkd3dResourcePresentStateTransition", "VKD3D_RESOURCE_PRESENT_STATE_TRANSITION"),
]

# Sizes only: type name, C type.
SIZES = []


def gen_layout():
    src = open(os.path.join(pkg, "src/sys/d3d12.aki")).read()
    # aki field names per struct, in order
    fields = {}
    for m in re.finditer(r"pub extern (struct|union) (\w+)(?: : \w+)? \{(.*?)\n\}", src, re.S):
        fields[m.group(2)] = re.findall(r"pub var (\w+):", m.group(3))
    c = []
    c.append("// GENERATED by tools/gen_d3d12.py layout: prints C's layout of the structs the port binds.")
    c.append("#define NOMINMAX\n#define INITGUID\n#define CINTERFACE\n#define COBJMACROS\n#define WIDL_C_INLINE_WRAPPERS")
    c.append("#include <stddef.h>\n#include <stdio.h>\n#include <vkd3d.h>\n#include <vkd3d_utils.h>")
    c.append("int main(void) {")
    rows = []
    for aki, cname, cfields in LAYOUT:
        if cfields is None:
            continue
        af = fields.get(aki)
        if af is None:
            raise SystemExit("no aki struct " + aki)
        if len(af) != len(cfields):
            raise SystemExit("%s: %d aki fields vs %d C fields: %s" % (aki, len(af), len(cfields), af))
        c.append('  printf("size %s %%zu %%zu\\n", sizeof(%s), _Alignof(%s));' % (aki, cname, cname))
        for a, cf in zip(af, cfields):
            c.append('  printf("off %s %s %%zu\\n", offsetof(%s, %s));' % (aki, a, cname, cf))
        rows.append((aki, af))
    for aki, cname, cf in UNION_LAYOUT:
        pass
    for aki_name, cexpr in CONSTS:
        c.append('  printf("const %s %%lld\\n", (long long)(%s));' % (aki_name, cexpr))
    c.append('  printf("size ClearValueData %zu %zu\\n", sizeof(((D3D12_CLEAR_VALUE*)0)->Color), _Alignof(D3D12_CLEAR_VALUE));')
    c.append("  return 0;\n}")
    cfile = os.path.join(here, "layout.c")
    open(cfile, "w").write("\n".join(c) + "\n")
    # compile and run
    cflags = subprocess.check_output(["pkg-config", "--cflags", "libvkd3d", "vulkan"]).decode().split()
    exe = "/tmp/ugui_d3d12_layout"
    subprocess.check_call(["clang", "-std=gnu11", "-w", cfile] + cflags + ["-o", exe])
    out = subprocess.check_output([exe]).decode().splitlines()
    sizes, offs, consts = {}, {}, {}
    for line in out:
        p = line.split()
        if p[0] == "size":
            sizes[p[1]] = (int(p[2]), int(p[3]))
        elif p[0] == "off":
            offs[(p[1], p[2])] = int(p[3])
        else:
            consts[p[1]] = int(p[2])
    # the test: C's numbers against the aki declarations
    t = []
    t.append("// GENERATED by tools/gen_d3d12.py layout from vkd3d's and Vulkan's headers; do not edit.")
    t.append("// C's size, alignment and field offsets of every struct sys::d3d12 declares, and the")
    t.append("// values of their constants, against the aki declarations.")
    t.append("")
    t.append("module layout_test;")
    t.append("")
    t.append("import {eprintln} from akikit::logging;")
    t.append("import * from ultragui_d3d12::sys::d3d12;")
    t.append("")
    t.append("static var failures: u32 = 0;")
    t.append("")
    t.append("fn check(what: str, got: usize, want: usize) {")
    t.append("  if (got != want) {")
    t.append("    eprintln(f\"FAIL {what}: got {got}, C has {want}\");")
    t.append("    unsafe { failures += 1; }")
    t.append("  }")
    t.append("}")
    t.append("")
    t.append("fn check_const(what: str, got: i64, want: i64) {")
    t.append("  if (got != want) {")
    t.append("    eprintln(f\"FAIL {what}: got {got}, C has {want}\");")
    t.append("    unsafe { failures += 1; }")
    t.append("  }")
    t.append("}")
    t.append("")
    t.append("pub fn main(): i32 {")
    for aki, cname, cf in LAYOUT:
        if cf is None:
            continue
        sz, al = sizes[aki]
        t.append('  check("sizeof %s", @size_of(%s), %d);' % (aki, aki, sz))
        t.append('  check("alignof %s", @align_of(%s), %d);' % (aki, aki, al))
    for aki, af in rows:
        for a_ in af:
            t.append('  check("offsetof %s.%s", @offset_of(%s, %s), %d);' % (aki, a_, aki, a_, offs[(aki, a_)]))
    for name, _ in CONSTS:
        t.append('  check_const("%s", %s as i64, %d);' % (name, name, consts[name]))
    t.append('  check("sizeof ClearValue.data", @size_of(ClearValueData), %d);' % sizes["ClearValueData"][0])
    t.append("  let n = unsafe { failures };")
    t.append("  return n == 0 ? 0 : 1;")
    t.append("}")
    open(os.path.join(pkg, "tests/layout_test.aki"), "w").write("\n".join(t) + "\n")
    print("wrote tools/layout.c and tests/layout_test.aki (%d structs, %d constants)" % (len(rows), len(consts)))


if mode == "com":
    path = os.path.join(pkg, "src/sys/d3d12.aki")
    text = open(path).read()
    a = text.index("// BEGIN GENERATED COM")
    b = text.index("// END GENERATED COM")
    head = text[:a] + "// BEGIN GENERATED COM (tools/gen_d3d12.py)\n"
    text = head + gen_com() + "\n" + text[b:]
    open(path, "w").write(text)
    print("rewrote the generated COM section")
elif mode == "layout":
    gen_layout()
