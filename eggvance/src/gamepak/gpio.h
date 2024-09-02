#pragma once

#include "base/int.h"

class Gpio
{
public:
    enum class Type
    {
        Detect,
        None,
        Rtc
    };

    Gpio();
    Gpio(Type type);
    virtual ~Gpio() = default;

    bool isReadable() const;
    bool isAccess(u32 addr) const;

    virtual void reset();

    u16 read(u32 addr);
    void write(u32 addr, u16 half);

    const Type type;

protected:
    virtual u16 readPort();
    virtual void writePort(u16 half);

    bool isGpioToGba(uint port) const;
    bool isGbaToGpio(uint port) const;

private:
    enum class Register
    {
        Data      = 0xC4,
        Direction = 0xC6,
        Control   = 0xC8
    };

    u16 maskGpioToGba() const;
    u16 maskGbaToGpio() const;

    u16 data      = 0;
    u16 direction = 0;
    u16 readable  = 0;
};
