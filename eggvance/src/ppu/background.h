#pragma once

#include "buffer.h"
#include "io.h"
#include "matrix.h"

class Background
{
public:
    Background(uint id);

    uint flag() const;

    const uint id;
    BackgroundOffset offset;
    BackgroundControl control;
    TransformationMatrix matrix;
    ScanlineDoubleBuffer<u16> buffer;
};
