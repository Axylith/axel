#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(push_constant) uniform PushConstants {
    vec2 screen_size;
    float pixel_range;
    float _padding;
} pc;

// Median of three values
float median(float a, float b, float c) {
    return max(min(a, b), min(max(a, b), c));
}

// Width of the antialiased edge in screen space, in pixels
float screen_px_range() {
    vec2 unit_range = vec2(pc.pixel_range) / vec2(textureSize(atlas, 0));
    vec2 screen_tex_size = vec2(1.0) / fwidth(in_uv);
    return max(0.5 * dot(unit_range, screen_tex_size), 1.0);
}

void main() {
    // Sample the MTSDF (RGB channels are the multi-channel SDF)
    vec3 msd = texture(atlas, in_uv).rgb;

    // Take median for the actual signed distance
    float sd = median(msd.r, msd.g, msd.b);

    // Convert distance to coverage (alpha)
    float px_range = screen_px_range();
    float screen_px_dist = px_range * (sd - 0.5);
    float opacity = clamp(screen_px_dist + 0.5, 0.0, 1.0);

    // Output color with computed alpha
    out_color = vec4(in_color.rgb, in_color.a * opacity);
}