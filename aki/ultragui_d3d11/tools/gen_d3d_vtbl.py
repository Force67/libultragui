#!/usr/bin/env python3
"""Generates src/sys/d3d11_vtbl.aki: the COM vtables (in the order of the
Windows SDK / DXVK headers) and the call wrappers for the Direct3D 11 and DXGI
methods ultragui calls.

  python3 tools/gen_d3d_vtbl.py <dir with d3d11.h and dxgi.h> > src/sys/d3d11_vtbl.aki

The slot order comes from the headers; the signatures of the methods in use are
the SIGS table below, every other slot is an opaque `Slot`.
"""
import re
import sys

HDR = sys.argv[1]
U = "*var Unknown"

# interface: (header, C name, aki struct name, this type)
IFACES = [
    ("Unknown", "d3d11.h", "IUnknown", "unknown"),
    ("Device", "d3d11.h", "ID3D11Device", "device"),
    ("DeviceContext", "d3d11.h", "ID3D11DeviceContext", "context"),
    ("RenderTargetView", "d3d11.h", "ID3D11RenderTargetView", "rtv"),
    ("SwapChain", "dxgi.h", "IDXGISwapChain", "swap_chain"),
    ("DxgiDevice", "dxgi.h", "IDXGIDevice", "dxgi_device"),
    ("DxgiAdapter", "dxgi.h", "IDXGIAdapter", "dxgi_adapter"),
    ("DxgiFactory", "dxgi.h", "IDXGIFactory", "dxgi_factory"),
]

SHADER_SET = ["*var Unknown", "*const *var Unknown", "u32"]  # shader, class instances, count
SIGS = {
    "IUnknown": {
        "QueryInterface": (["*const Guid", "*var *var Unknown"], "Hresult"),
        "AddRef": ([], "u32"),
        "Release": ([], "u32"),
    },
    "ID3D11Device": {
        "QueryInterface": (["*const Guid", "*var *var Unknown"], "Hresult"),
        "Release": ([], "u32"),
        "CreateBuffer": (["*const BufferDesc", "*const SubresourceData", "*var *var Unknown"], "Hresult"),
        "CreateTexture2D": (["*const Texture2dDesc", "*const SubresourceData", "*var *var Unknown"], "Hresult"),
        "CreateShaderResourceView": ([U, "*const ShaderResourceViewDesc", "*var *var Unknown"], "Hresult"),
        "CreateRenderTargetView": ([U, "*const RenderTargetViewDesc", "*var *var Unknown"], "Hresult"),
        "CreateInputLayout": (["*const InputElementDesc", "u32", "*const u8", "usize", "*var *var Unknown"], "Hresult"),
        "CreateVertexShader": (["*const u8", "usize", U, "*var *var Unknown"], "Hresult"),
        "CreatePixelShader": (["*const u8", "usize", U, "*var *var Unknown"], "Hresult"),
        "CreateBlendState": (["*const BlendDesc", "*var *var Unknown"], "Hresult"),
        "CreateDepthStencilState": (["*const DepthStencilDesc", "*var *var Unknown"], "Hresult"),
        "CreateRasterizerState": (["*const RasterizerDesc", "*var *var Unknown"], "Hresult"),
        "CreateSamplerState": (["*const SamplerDesc", "*var *var Unknown"], "Hresult"),
    },
    "ID3D11DeviceContext": {
        "Release": ([], "u32"),
        "VSSetConstantBuffers": (["u32", "u32", "*const *var Unknown"], "()"),
        "PSSetShaderResources": (["u32", "u32", "*const *var Unknown"], "()"),
        "PSSetShader": (SHADER_SET, "()"),
        "PSSetSamplers": (["u32", "u32", "*const *var Unknown"], "()"),
        "VSSetShader": (SHADER_SET, "()"),
        "DrawIndexed": (["u32", "u32", "i32"], "()"),
        "Draw": (["u32", "u32"], "()"),
        "Map": ([U, "u32", "u32", "u32", "*var MappedSubresource"], "Hresult"),
        "Unmap": ([U, "u32"], "()"),
        "IASetInputLayout": ([U], "()"),
        "IASetVertexBuffers": (["u32", "u32", "*const *var Unknown", "*const u32", "*const u32"], "()"),
        "IASetIndexBuffer": ([U, "u32", "u32"], "()"),
        "GSSetShader": (SHADER_SET, "()"),
        "IASetPrimitiveTopology": (["u32"], "()"),
        "OMSetRenderTargets": (["u32", "*const *var Unknown", U], "()"),
        "OMSetBlendState": ([U, "*const f32", "u32"], "()"),
        "OMSetDepthStencilState": ([U, "u32"], "()"),
        "RSSetState": ([U], "()"),
        "RSSetViewports": (["u32", "*const Viewport"], "()"),
        "RSSetScissorRects": (["u32", "*const D3dRect"], "()"),
        "UpdateSubresource": ([U, "u32", "*const u8", "*const u8", "u32", "u32"], "()"),
        "ClearRenderTargetView": ([U, "*const f32"], "()"),
        "HSSetShader": (SHADER_SET, "()"),
        "DSSetShader": (SHADER_SET, "()"),
        "VSGetConstantBuffers": (["u32", "u32", "*var *var Unknown"], "()"),
        "PSGetShaderResources": (["u32", "u32", "*var *var Unknown"], "()"),
        "PSGetShader": (["*var *var Unknown", "*var *var Unknown", "*var u32"], "()"),
        "PSGetSamplers": (["u32", "u32", "*var *var Unknown"], "()"),
        "VSGetShader": (["*var *var Unknown", "*var *var Unknown", "*var u32"], "()"),
        "GSGetShader": (["*var *var Unknown", "*var *var Unknown", "*var u32"], "()"),
        "HSGetShader": (["*var *var Unknown", "*var *var Unknown", "*var u32"], "()"),
        "DSGetShader": (["*var *var Unknown", "*var *var Unknown", "*var u32"], "()"),
        "IAGetInputLayout": (["*var *var Unknown"], "()"),
        "IAGetVertexBuffers": (["u32", "u32", "*var *var Unknown", "*var u32", "*var u32"], "()"),
        "IAGetIndexBuffer": (["*var *var Unknown", "*var u32", "*var u32"], "()"),
        "IAGetPrimitiveTopology": (["*var u32"], "()"),
        "OMGetRenderTargets": (["u32", "*var *var Unknown", "*var *var Unknown"], "()"),
        "OMGetBlendState": (["*var *var Unknown", "*var f32", "*var u32"], "()"),
        "OMGetDepthStencilState": (["*var *var Unknown", "*var u32"], "()"),
        "RSGetState": (["*var *var Unknown"], "()"),
        "RSGetViewports": (["*var u32", "*var Viewport"], "()"),
        "RSGetScissorRects": (["*var u32", "*var D3dRect"], "()"),
        "Flush": ([], "()"),
    },
    "ID3D11RenderTargetView": {
        "GetDesc": (["*var RenderTargetViewDesc"], "()"),
    },
    "IDXGISwapChain": {
        "Present": (["u32", "u32"], "Hresult"),
        "GetBuffer": (["u32", "*const Guid", "*var *var Unknown"], "Hresult"),
        "ResizeBuffers": (["u32", "u32", "u32", "u32", "u32"], "Hresult"),
    },
    "IDXGIDevice": {
        "GetAdapter": (["*var *var Unknown"], "Hresult"),
    },
    "IDXGIAdapter": {
        "GetParent": (["*const Guid", "*var *var Unknown"], "Hresult"),
    },
    "IDXGIFactory": {
        "CreateSwapChain": ([U, "*const DxgiSwapChainDesc", "*var *var Unknown"], "Hresult"),
    },
}


def snake(name):
    name = name.replace("2D", "2d")
    s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])", "_", name)
    return s.lower()


def slots(header, cname):
    if cname == "IUnknown":
        return ["QueryInterface", "AddRef", "Release"]
    s = open("%s/%s" % (HDR, header)).read()
    m = re.search(r"typedef struct %sVtbl \{(.*?)\} %sVtbl;" % (cname, cname), s, re.S)
    return re.findall(r"\(STDMETHODCALLTYPE \*(\w+)\)", m.group(1))


out = []
w = out.append
w("// Generated by tools/gen_d3d_vtbl.py from the d3d11.h and dxgi.h of the Windows SDK /")
w("// DXVK-native: do not edit. See sys/d3d11.aki.")
w("//")
w("// One vtable struct per interface, slots in header order. Slots ultragui calls have")
w("// their signature, with the object itself as the first parameter; every other slot is")
w("// an opaque `Slot`. Beside each vtable, the call wrappers (the COBJMACROS of the C")
w("// headers): `device_create_buffer(device, ...)` and so on. They are `unsafe`: the")
w("// pointers are COM pointers and out-parameters.")
w("")
w("module ultragui_d3d11::sys::d3d11_vtbl;")
w("")
w("import {BlendDesc, BufferDesc, D3dRect, DepthStencilDesc, DxgiSwapChainDesc, Guid, Hresult, InputElementDesc,")
w("  MappedSubresource, RasterizerDesc, RenderTargetViewDesc, SamplerDesc, ShaderResourceViewDesc, SubresourceData,")
w("  Texture2dDesc, Viewport} from ultragui_d3d11::sys::d3d11;")
w("")
w("// An opaque slot: never called.")
w("pub type Slot = extern fn(*var Unknown): ();")
w("")
w("// Any COM object (buffer, texture, view, shader, state): the vtable begins with IUnknown's.")
w("pub extern struct Unknown {")
w("  pub var vtbl: *const UnknownVtbl")
w("}")
for iface, header, cname, pre in IFACES[1:]:
    w("pub extern struct %s {" % iface)
    w("  pub var vtbl: *const %sVtbl" % iface)
    w("}")
w("")

THIS = {i[0]: "*var " + i[0] for i in IFACES}
for iface, header, cname, pre in IFACES:
    names = slots(header, cname)
    sigs = SIGS[cname]
    w("pub extern struct %sVtbl {" % iface)
    for n in names:
        if n in sigs:
            params, ret = sigs[n]
            sig = ", ".join([THIS[iface]] + params)
            w("  pub var %s: extern fn(%s): %s" % (snake(n), sig, ret))
        else:
            w("  pub var %s: Slot" % snake(n))
    w("}")
    w("")
    for n in names:
        if n not in sigs:
            continue
        params, ret = sigs[n]
        if iface != "Unknown" and n in ("QueryInterface", "AddRef", "Release") and iface not in ("Device", "DeviceContext"):
            continue
        plist = ", ".join(["this: " + THIS[iface]] + ["a%d: %s" % (i, p) for i, p in enumerate(params)])
        args = ", ".join(["this"] + ["a%d" % i for i in range(len(params))])
        fn = "%s_%s" % (pre, snake(n))
        if ret == "()":
            w("pub unsafe fn %s(%s) {" % (fn, plist))
            w("  unsafe ((*(*this).vtbl).%s)(%s);" % (snake(n), args))
            w("}")
        else:
            w("pub unsafe fn %s(%s): %s => unsafe ((*(*this).vtbl).%s)(%s);" % (fn, plist, ret, snake(n), args))
    w("")

w("// D3D11CreateDevice, from d3d11.dll (the SDK) or libdxvk_d3d11 (DXVK-native).")
w("pub extern(\"D3D11CreateDevice\") fn d3d11_create_device(adapter: *var Unknown, driver_type: u32, software: *var u8, flags: u32,")
w("  feature_levels: *const u32, levels: u32, sdk_version: u32, device: *var *var Device, achieved_level: *var u32,")
w("  immediate_context: *var *var DeviceContext): Hresult;")
w("")
w("// Release of any COM object; the vtables of every interface begin with IUnknown's.")
w("pub unsafe fn release<T>(object: *var T): u32 {")
w("  unsafe {")
w("    let u = object as *var Unknown;")
w("    return ((*(*u).vtbl).release)(u);")
w("  }")
w("}")
w("")
w("pub unsafe fn add_ref<T>(object: *var T): u32 {")
w("  unsafe {")
w("    let u = object as *var Unknown;")
w("    return ((*(*u).vtbl).add_ref)(u);")
w("  }")
w("}")
sys.stdout.write("\n".join(out) + "\n")
