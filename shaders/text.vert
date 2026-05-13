#version 450

// Per-vertex input
layout(location = 0) in vec2 in_pos;     // screen-space position in pixels
layout(location = 1) in vec2 in_uv;      // atlas UV coords [0, 1]
layout(location = 2) in vec4 in_color;   // text color

// To fragment shader
layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

// Push constants: screen dimensions for converting pixel coords to NDC
layout(push_constant) uniform PushConstants {
    vec2 screen_size;     // (width, height) in pixels
    float pixel_range;    // MTSDF pxrange (we baked with 4)
    float _padding;
} pc;

void main() {
    // Convert pixel position to NDC (Normalized Device Coordinates: [-1, 1])
    // Y is flipped because Vulkan's Y points down in screen space but up in NDC
    vec2 ndc = (in_pos / pc.screen_size) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    out_uv = in_uv;
    out_color = in_color;
}