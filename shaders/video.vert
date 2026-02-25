#version 450

layout(location = 0) out vec2 frag_uv;

void main() {
    // Fullscreen triangle from vertex index - no vertex buffer needed.
    // Triangle covers clip space [-1,-1] to [3,1] / [-1,3]; clipped to screen.
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    frag_uv = uvs[gl_VertexIndex];
}
