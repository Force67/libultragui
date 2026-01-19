#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in float frag_corner_radius;
layout(location = 3) in vec2 frag_half_size;

layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

float sdf_rounded_rect(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec4 tex_color = texture(tex_sampler, frag_uv);
    vec4 color = frag_color * tex_color;

    // SDF-based alpha for rounded rects
    if (frag_half_size.x > 0.0 && frag_half_size.y > 0.0) {
        // Map UV (0-1) to local coordinates centered on rect
        vec2 local = (frag_uv * 2.0 - 1.0) * frag_half_size;
        float d = sdf_rounded_rect(local, frag_half_size, frag_corner_radius);
        float aa = fwidth(d) * 0.75;
        float alpha = 1.0 - smoothstep(-aa, aa, d);
        color.a *= alpha;
    }

    out_color = color;
}
