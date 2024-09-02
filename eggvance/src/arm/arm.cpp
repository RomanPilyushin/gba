#include "arm.h"

#include "decode.h"
#include "dma/dma.h"
#include "scheduler/scheduler.h"
#include "timer/timer.h"

Arm::Arm()
{
    interrupt.delay = [this](u64 late)
    {
        state |= State::Irq;
    };
}

void Arm::init()
{
    flushWord();
    pc += 4;
}

void Arm::run(u64 cycles)
{
    target += cycles;

    while (scheduler.now < target)
    {
        switch (state)
        {
        SHELL_CASE16(0, dispatch<kLabel>())

        default:
            SHELL_UNREACHABLE;
            break;
        }
    }
}

template<uint kState>
void Arm::dispatch()
{
    while (scheduler.now < target && state == kState)
    {
        if (kState & State::Dma)
        {
            dma.run();
        }
        else if (kState & State::Halt)
        {
            scheduler.run(std::min(target - scheduler.now, scheduler.next - scheduler.now));
        }
        else
        {
            if ((kState & State::Irq) && !cpsr.i)
            {
                interruptHw();
            }
            else
            {
                if (kState & State::Thumb)
                {
                    u16 instr = pipe[0];

                    pipe[0] = pipe[1];
                    pipe[1] = readHalf(pc, pipe.access);
                    pipe.access = Access::Sequential;

                    std::invoke(instr_thumb[hashThumb(instr)], this, instr);
                }
                else
                {
                    u32 instr = pipe[0];

                    pipe[0] = pipe[1];
                    pipe[1] = readWord(pc, pipe.access);
                    pipe.access = Access::Sequential;

                    if (cpsr.check(instr >> 28))
                    {
                        std::invoke(instr_arm[hashArm(instr)], this, instr);
                    }
                }
            }
            pc += cpsr.size();
        }
    }
}

void Arm::flushHalf()
{
    pc &= ~0x1;
    pipe[0] = readHalf(pc + 0, Access::NonSequential);
    pipe[1] = readHalf(pc + 2, Access::Sequential);
    pipe.access = Access::Sequential;
    pc += 2;
}

void Arm::flushWord()
{
    pc &= ~0x3;
    pipe[0] = readWord(pc + 0, Access::NonSequential);
    pipe[1] = readWord(pc + 4, Access::Sequential);
    pipe.access = Access::Sequential;
    pc += 4;
}
