#pragma once

#include "base/filesystem.h"
#include "base/ram.h"

class Bios
{
public:
    static constexpr auto kSize = 16 * 1024;

    static void init(const fs::path& path);

    u8  readByte(u32 addr);
    u16 readHalf(u32 addr);
    u32 readWord(u32 addr);

private:
    static Ram<kSize> data;
    static Ram<kSize> replacement;

    template<typename Integral>
    Integral read(u32 addr);

    u32 latch = 0xE129'F000;
};
