#pragma once
#include <cstdint>

struct Glyph {
    uint32_t codepoint;
    float advance;

    float plane_left, plane_bottom, plane_right, plane_top;

    float atlas_left, atlas_right, atlas_bottom, atlas_top;
};

struct  Font
{
    uint32_t atlas_width;
    uint32_t atlas_height;
    float distance_range;
    float em_size_px;

    float line_height;
    float ascender;
    float underline_y;
    float underline_thickness;

    Glyph glyph[128]
};
