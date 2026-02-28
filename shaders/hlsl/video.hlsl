// ultragui video shader: D3D12 HLSL port of video.vert + video.frag
// Compile: dxc -T vs_6_0 -E VSMain -Fo video_vs.cso video.hlsl
//          dxc -T ps_6_0 -E PSMain -Fo video_ps.cso video.hlsl

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
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
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

float4 PSMain(VSOutput input) : SV_Target {
    float y  = tex_y.Sample(samp_linear, input.uv).r;
    float cb = tex_cb.Sample(samp_linear, input.uv).r;
    float cr = tex_cr.Sample(samp_linear, input.uv).r;

    // BT.601 YCbCr to RGB conversion (from pl_mpeg documentation)
    float4 ycbcr = float4(y, cb, cr, 1.0);
    float r = dot(ycbcr, float4(1.16438,  0.00000,  1.59603, -0.87079));
    float g = dot(ycbcr, float4(1.16438, -0.39176, -0.81297,  0.52959));
    float b = dot(ycbcr, float4(1.16438,  2.01723,  0.00000, -1.08139));

    return float4(saturate(r), saturate(g), saturate(b), 1.0);
}
