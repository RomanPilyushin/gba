#include "io.h"

#include "arm/arm.h"
#include "base/config.h"

RemoteControl::RemoteControl()
{
    if (config.bios_skip)
    {
        write(1, 0x80);
    }
}

void SioControl::write(uint index, u8 byte)
{
    Register::write(index, byte);

    if (index == 0)
    {
        constexpr auto kEnabled = 1 << 7;

        if (data & kEnabled)
        {
            if (irq)
                arm.raise(Irq::Serial);

            data &= ~kEnabled;
        }
    }
    else
    {
        irq = bit::seq<6, 1>(byte);
    }
}
