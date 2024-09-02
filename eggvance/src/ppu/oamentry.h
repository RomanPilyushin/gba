#pragma once

#include "point.h"
#include "base/int.h"

class OamEntry
{
public:
    OamEntry();

    void writeAttr0(u16 half);
    void writeAttr1(u16 half);
    void writeAttr2(u16 half);

    bool isVisible(uint line) const;
    uint tileBytes() const;
    uint tilesPerRow(uint layout) const;
    uint paletteBank() const;
    uint cycles() const;

    uint affine      = 0;
    uint disabled    = 0;
    uint object_mode = 0;
    uint mosaic      = 0;
    uint color_mode  = 0;
    uint matrix      = 0;
    uint priority    = 0;
    uint base_addr   = 0;

    Point flip;
    Point origin;
    Point center;
    Point sprite_size;
    Point screen_size;

private:
    void compute();

    uint double_size = 0;
    uint shape       = 0;
    uint size        = 0;
    uint base_tile   = 0;
    uint bank        = 0;
    uint visible_x   = 0;
};
