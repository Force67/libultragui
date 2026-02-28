// ultragui quad shader: D3D12 HLSL port of quad.vert + quad.frag
// Compile: dxc -T vs_6_0 -E VSMain -Fo quad_vs.cso quad.hlsl
//          dxc -T ps_6_0 -E PSMain -Fo quad_ps.cso quad.hlsl

cbuffer PushConstants : register(b0) {
    float2 scale;
    float2 translate;
};

Texture2D tex : register(t0);
SamplerState samp_linear : register(s0);

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
    float4 pos          : SV_Position;
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    float4 color2       : COLOR1;
    float4 corner_radii : TEXCOORD1;
    float  softness     : TEXCOORD2;
    float2 half_size    : TEXCOORD3;
    float  border_width : TEXCOORD4;
    float4 border_color : COLOR2;
};

// sRGB EOTF: sRGB -> linear
float3 srgb_to_linear(float3 c) {
    return lerp(c / 12.92, pow((c + 0.055) / 1.055, 2.4), step(0.04045, c));
}

float4 unpack_color(uint c) {
    float4 col = float4(
        float(c & 0xFFu) / 255.0,
        float((c >> 8u) & 0xFFu) / 255.0,
        float((c >> 16u) & 0xFFu) / 255.0,
        float((c >> 24u) & 0xFFu) / 255.0
    );
    col.rgb = srgb_to_linear(col.rgb);
    return col;
}

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.pos = float4(input.pos * scale + translate, 0.0, 1.0);
    o.uv = input.uv;
    o.color = unpack_color(input.color);
    o.color2 = unpack_color(input.color2);
    o.corner_radii = float4(
        float(input.corner_radii & 0xFFu),
        float((input.corner_radii >> 8u) & 0xFFu),
        float((input.corner_radii >> 16u) & 0xFFu),
        float((input.corner_radii >> 24u) & 0xFFu)
    );
    o.softness = input.softness;
    o.half_size = input.half_size;
    o.border_width = input.border_width;
    o.border_color = unpack_color(input.border_color);
    return o;
}

float sdf_rounded_rect_4(float2 p, float2 b, float4 radii) {
    // radii = (tl, tr, br, bl)
    float radius = (p.x > 0.0) ? ((p.y > 0.0) ? radii.z : radii.y) : ((p.y > 0.0) ? radii.w : radii.x);
    float2 q = abs(p) - b + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float4 PSMain(VSOutput input) : SV_Target {
    float4 tex_color = tex.Sample(samp_linear, input.uv);
    tex_color.rgb = srgb_to_linear(tex_color.rgb);

    // Linear gradient along V axis
    float4 base_color = lerp(input.color, input.color2, input.uv.y);
    float4 color = base_color * tex_color;

    // SDF-based alpha for rounded rects
    if (input.half_size.x > 0.0 && input.half_size.y > 0.0) {
        float2 local = (input.uv * 2.0 - 1.0) * input.half_size;
        float d = sdf_rounded_rect_4(local, input.half_size, input.corner_radii);

        float soft = abs(input.softness);
        float aa = max(fwidth(d) * 0.75, soft);
        float alpha;
        if (input.softness < 0.0) {
            alpha = smoothstep(-aa, 0.0, d);
        } else {
            alpha = 1.0 - smoothstep(-aa, aa, d);
        }
        color.a *= alpha;

        // Border rendering
        if (input.border_width > 0.0 && input.border_color.a > 0.0) {
            float2 inner_half = input.half_size - float2(input.border_width, input.border_width);
            float4 inner_radii = max(input.corner_radii - float4(input.border_width, input.border_width,
                                     input.border_width, input.border_width), float4(0, 0, 0, 0));
            float d_inner = sdf_rounded_rect_4(local, inner_half, inner_radii);
            float inner_aa = fwidth(d_inner) * 0.75;
            float inner_alpha = 1.0 - smoothstep(-inner_aa, inner_aa, d_inner);

            float border_mask = alpha * (1.0 - inner_alpha);
            float4 border_col = input.border_color;
            border_col.a *= border_mask;

            float4 fill = color;
            fill.a *= inner_alpha;

            // Pre-multiplied alpha compositing
            color = fill + border_col * (1.0 - fill.a);
            color.a = fill.a + border_col.a * (1.0 - fill.a);
        }
    }

    return color;
}
