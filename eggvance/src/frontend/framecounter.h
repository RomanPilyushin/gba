#pragma once

#include <chrono>
#include <optional>

#include "base/int.h"

class FrameCounter
{
public:
    FrameCounter();
    FrameCounter& operator++();

    void reset();
    void queueReset();

    std::optional<double> fps();

private:
    using Clock = std::chrono::high_resolution_clock;
    using Time  = std::chrono::high_resolution_clock::time_point;

    Time begin;
    uint count = 0;
    bool queue_reset = false;
};
