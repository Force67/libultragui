#version 450

layout(push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
}
pc;

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in uint in_color;
layout(location = 3) in uint in_color2;
layout(location = 4) in uint in_corner_radii;
layout(location = 5) in float in_softness;
layout(location = 6) in vec2 in_half_size;
layout(location = 7) in float in_border_width;
layout(location = 8) in uint in_border_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out vec4 frag_color2;
layout(location = 3) out vec4 frag_corner_radii;
layout(location = 4) out float frag_softness;
layout(location = 5) out vec2 frag_half_size;
layout(location = 6) out float frag_border_width;
layout(location = 7) out vec4 frag_border_color;

vec4 unpack_color(uint c) {
    return vec4(
        float(c & 0xFFu) / 255.0,
        float((c >> 8) & 0xFFu) / 255.0,
        float((c >> 16) & 0xFFu) / 255.0,
        float((c >> 24) & 0xFFu) / 255.0
    );
}

void main() {
    gl_Position = vec4(in_pos * pc.scale + pc.translate, 0.0, 1.0);

    frag_uv = in_uv;
    frag_color = unpack_color(in_color);
    frag_color2 = unpack_color(in_color2);
    frag_corner_radii = vec4(
        float(in_corner_radii & 0xFFu),
        float((in_corner_radii >> 8) & 0xFFu),
        float((in_corner_radii >> 16) & 0xFFu),
        float((in_corner_radii >> 24) & 0xFFu)
    );
    frag_softness = in_softness;
    frag_half_size = in_half_size;
    frag_border_width = in_border_width;
    frag_border_color = unpack_color(in_border_color);
}
