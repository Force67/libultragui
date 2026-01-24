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
layout(location = 4) in float in_corner_radius;
layout(location = 5) in float in_softness;
layout(location = 6) in vec2 in_half_size;
layout(location = 7) in float in_border_width;
layout(location = 8) in uint in_border_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;

void main() {
    gl_Position = vec4(in_pos * pc.scale + pc.translate, 0.0, 1.0);
    frag_uv = in_uv;

    frag_color =
        vec4(float(in_color & 0xFFu) / 255.0, float((in_color >> 8) & 0xFFu) / 255.0,
             float((in_color >> 16) & 0xFFu) / 255.0, float((in_color >> 24) & 0xFFu) / 255.0);
}
