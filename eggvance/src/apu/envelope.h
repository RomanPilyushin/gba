#pragma once

#include "base/int.h"

class Envelope
{
public:
    void init();
    void tick();
    void write(u8 byte);
    uint enabled() const;

    uint timer    = 0;
    uint volume   = 0;
    uint period   = 0;
    uint increase = 0;
    uint initial  = 0;
};
