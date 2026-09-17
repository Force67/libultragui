#version 450

layout(set = 0, binding = 0) uniform sampler2D tex_y;
layout(set = 0, binding = 1) uniform sampler2D tex_cb;
layout(set = 0, binding = 2) uniform sampler2D tex_cr;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

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

    // BT.601 gives gamma-encoded RGB, and this pass writes through an sRGB
    // render target view. Decode so the hardware's encode puts the original
    // values back into the texture the UI later samples.
    vec3 rgb = clamp(vec3(r, g, b), 0.0, 1.0);
    out_color = vec4(srgb_to_linear(rgb), 1.0);
}
