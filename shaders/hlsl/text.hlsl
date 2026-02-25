// ultragui text shader - D3D12 HLSL port of text.vert + text.frag
// Compile: dxc -T vs_6_0 -E VSMain -Fo text_vs.cso text.hlsl
//          dxc -T ps_6_0 -E PSMain -Fo text_ps.cso text.hlsl

cbuffer PushConstants : register(b0) {
    float2 scale;
    float2 translate;
};

Texture2D tex : register(t0);
SamplerState samp_nearest : register(s1);

struct VSInput {
    float2 pos          : POSITION;
    float2 uv           : TEXCOORD0;
    uint   color        : COLOR0;
    uint   color2       : COLOR1;
    uint   corner_radii : BLENDINDICES0;
    float  softness     : BLENDWEIGHT0;
    float2 half_size    : TEXCOORD1;
    float  border_width : BLENDWEIGHT1;
    uint   border_color : COLOR2;
};

struct VSOutput {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float3 srgb_to_linear(float3 c) {
    return lerp(c / 12.92, pow((c + 0.055) / 1.055, 2.4), step(0.04045, c));
}

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.pos = float4(input.pos * scale + translate, 0.0, 1.0);
    o.uv = input.uv;

    float4 col = float4(
        float(input.color & 0xFFu) / 255.0,
        float((input.color >> 8u) & 0xFFu) / 255.0,
        float((input.color >> 16u) & 0xFFu) / 255.0,
        float((input.color >> 24u) & 0xFFu) / 255.0
    );
    col.rgb = srgb_to_linear(col.rgb);
    o.color = col;
    return o;
}

float4 PSMain(VSOutput input) : SV_Target {
    float alpha = tex.Sample(samp_nearest, input.uv).r;
    return float4(input.color.rgb, input.color.a * alpha);
}
