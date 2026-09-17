// ultragui video shader: HLSL port of video.vert + video.frag. Both D3D backends
// share it. D3D12 compiles it to DXIL at build time:
//   dxc -T vs_6_0 -E VSMain -Fo video_vs.cso video.hlsl
//   dxc -T ps_6_0 -E PSMain -Fo video_ps.cso video.hlsl
// D3D11 needs DXBC instead, so it compiles this text at startup from the copy
// CMake embeds into ugui_hlsl_embedded.h. Keep this file the only source.

Texture2D tex_y  : register(t0);
Texture2D tex_cb : register(t1);
Texture2D tex_cr : register(t2);
SamplerState samp_linear : register(s0);

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(uint vid : SV_VertexID) {
    // Fullscreen triangle from vertex index: no vertex buffer needed.
    // D3D clip space has +Y up while texture V runs down, and this pass writes
    // clip space directly instead of going through the quad shader's
    // Y-negating projection, so the triangle is flipped here to keep uv (0,0)
    // on the top-left texel.
    float2 positions[3] = {
        float2(-1.0,  1.0),
        float2( 3.0,  1.0),
        float2(-1.0, -3.0)
    };
    float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(2.0, 0.0),
        float2(0.0, 2.0)
    };

    VSOutput o;
    o.pos = float4(positions[vid], 0.0, 1.0);
    o.uv = uvs[vid];
    return o;
}

float3 srgb_to_linear(float3 c) {
    return lerp(c / 12.92, pow((c + 0.055) / 1.055, 2.4), step(0.04045, c));
}

float4 PSMain(VSOutput input) : SV_Target {
    float y  = tex_y.Sample(samp_linear, input.uv).r;
    float cb = tex_cb.Sample(samp_linear, input.uv).r;
    float cr = tex_cr.Sample(samp_linear, input.uv).r;

    // BT.601 YCbCr to RGB conversion (from pl_mpeg documentation)
    float4 ycbcr = float4(y, cb, cr, 1.0);
    float r = dot(ycbcr, float4(1.16438,  0.00000,  1.59603, -0.87079));
    float g = dot(ycbcr, float4(1.16438, -0.39176, -0.81297,  0.52959));
    float b = dot(ycbcr, float4(1.16438,  2.01723,  0.00000, -1.08139));

    // BT.601 gives gamma-encoded RGB, and this pass writes through an sRGB
    // render target view. Decode so the hardware's encode puts the original
    // values back into the texture the UI later samples.
    float3 rgb = saturate(float3(r, g, b));
    return float4(srgb_to_linear(rgb), 1.0);
}
