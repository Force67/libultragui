#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in vec4 frag_color2;
layout(location = 3) in vec4 frag_corner_radii;
layout(location = 4) in float frag_softness;
layout(location = 5) in vec2 frag_half_size;
layout(location = 6) in float frag_border_width;
layout(location = 7) in vec4 frag_border_color;

layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

float sdf_rounded_rect_4(vec2 p, vec2 b, vec4 radii) {
    // radii = (tl, tr, br, bl)
    // top-left: p.x < 0, p.y < 0 -> radii.x (tl)
    // top-right: p.x > 0, p.y < 0 -> radii.y (tr)
    // bottom-right: p.x > 0, p.y > 0 -> radii.z (br)
    // bottom-left: p.x < 0, p.y > 0 -> radii.w (bl)
    float radius = (p.x > 0.0) ? ((p.y > 0.0) ? radii.z : radii.y) : ((p.y > 0.0) ? radii.w : radii.x);
    vec2 q = abs(p) - b + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void main() {
    vec4 tex_color = texture(tex_sampler, frag_uv);
    // Texture stores sRGB data in UNORM format: linearize RGB for correct blending.
    // Alpha channel is always linear.
    tex_color.rgb = srgb_to_linear(tex_color.rgb);

    // Linear gradient: interpolate between color and color2 along V axis
    vec4 base_color = mix(frag_color, frag_color2, frag_uv.y);
    vec4 color = base_color * tex_color;

    // SDF-based alpha for rounded rects
    if (frag_half_size.x > 0.0 && frag_half_size.y > 0.0) {
        // Map UV (0-1) to local coordinates centered on rect
        vec2 local = (frag_uv * 2.0 - 1.0) * frag_half_size;
        float d = sdf_rounded_rect_4(local, frag_half_size, frag_corner_radii);

        // Softness: positive = outer blur/shadow, negative = inset shadow
        float soft = abs(frag_softness);
        float aa = max(fwidth(d) * 0.75, soft);
        float alpha;
        if (frag_softness < 0.0) {
            // Inset shadow: fade from edge inward
            alpha = smoothstep(-aa, 0.0, d);
        } else {
            alpha = 1.0 - smoothstep(-aa, aa, d);
        }
        color.a *= alpha;

        // Border rendering: draw border as a ring using inner SDF
        if (frag_border_width > 0.0 && frag_border_color.a > 0.0) {
            vec2 inner_half = frag_half_size - vec2(frag_border_width);
            vec4 inner_radii = max(frag_corner_radii - vec4(frag_border_width), vec4(0.0));
            float d_inner = sdf_rounded_rect_4(local, inner_half, inner_radii);
            float inner_aa = fwidth(d_inner) * 0.75;
            float inner_alpha = 1.0 - smoothstep(-inner_aa, inner_aa, d_inner);

            // Border region: outside inner, inside outer
            float border_mask = alpha * (1.0 - inner_alpha);
            vec4 border_col = frag_border_color;
            border_col.a *= border_mask;

            // Composite: inner fill + border ring
            vec4 fill = color;
            fill.a *= inner_alpha;

            // Pre-multiplied alpha compositing
            color = fill + border_col * (1.0 - fill.a);
            color.a = fill.a + border_col.a * (1.0 - fill.a);
        }
    }

    out_color = color;
}
