#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in vec4 frag_color2;
layout(location = 3) in float frag_corner_radius;
layout(location = 4) in float frag_softness;
layout(location = 5) in vec2 frag_half_size;
layout(location = 6) in float frag_border_width;
layout(location = 7) in vec4 frag_border_color;

layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

float sdf_rounded_rect(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec4 tex_color = texture(tex_sampler, frag_uv);

    // Linear gradient: interpolate between color and color2 along V axis
    vec4 base_color = mix(frag_color, frag_color2, frag_uv.y);
    vec4 color = base_color * tex_color;

    // SDF-based alpha for rounded rects
    if (frag_half_size.x > 0.0 && frag_half_size.y > 0.0) {
        // Map UV (0-1) to local coordinates centered on rect
        vec2 local = (frag_uv * 2.0 - 1.0) * frag_half_size;
        float d = sdf_rounded_rect(local, frag_half_size, frag_corner_radius);

        // Anti-aliasing width - use softness for shadow blur, otherwise crisp
        float aa = max(fwidth(d) * 0.75, frag_softness);
        float alpha = 1.0 - smoothstep(-aa, aa, d);
        color.a *= alpha;

        // Border rendering: draw border as a ring using inner SDF
        if (frag_border_width > 0.0 && frag_border_color.a > 0.0) {
            vec2 inner_half = frag_half_size - vec2(frag_border_width);
            float inner_radius = max(frag_corner_radius - frag_border_width, 0.0);
            float d_inner = sdf_rounded_rect(local, inner_half, inner_radius);
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
