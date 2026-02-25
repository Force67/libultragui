#version 450

layout(set = 0, binding = 0) uniform sampler2D tex_y;
layout(set = 0, binding = 1) uniform sampler2D tex_cb;
layout(set = 0, binding = 2) uniform sampler2D tex_cr;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

void main() {
    float y  = texture(tex_y,  frag_uv).r;
    float cb = texture(tex_cb, frag_uv).r;
    float cr = texture(tex_cr, frag_uv).r;

    // BT.601 YCbCr to RGB conversion matrix (from pl_mpeg documentation).
    // Input: Y in [16/255, 235/255], Cb/Cr in [16/255, 240/255].
    // The constants handle the offset and scaling.
    vec4 ycbcr = vec4(y, cb, cr, 1.0);
    float r = dot(ycbcr, vec4(1.16438,  0.00000,  1.59603, -0.87079));
    float g = dot(ycbcr, vec4(1.16438, -0.39176, -0.81297,  0.52959));
    float b = dot(ycbcr, vec4(1.16438,  2.01723,  0.00000, -1.08139));

    out_color = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
}
