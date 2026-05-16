#version 450

// Per-vertex input
layout(location = 0) in vec2 in_pos;     // screen-space position in pixels
layout(location = 1) in vec4 in_color;   // RGBA, 0-1 per channel

// To fragment shader
layout(location = 0) out vec4 out_color;

// Push constants: screen dimensions for converting pixel coords to NDC.
// Same layout convention as text.vert so the two pipelines stay consistent.
layout(push_constant) uniform PushConstants {
    vec2 screen_size;     // (width, height) in pixels
    vec2 _padding;
} pc;

void main() {
    // Pixel space -> NDC [-1, 1]. Y points down in screen space, same as text.
    vec2 ndc = (in_pos / pc.screen_size) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    out_color = in_color;
}
