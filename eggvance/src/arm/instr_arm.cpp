#include "arm.h"

#include "decode.h"

template<u32 kInstr>
void Arm::Arm_BranchExchange(u32 instr)
{
    uint rn = bit::seq<0, 4>(instr);

    pc = regs[rn];

    if ((cpsr.t = pc & 0x1))
    {
        flushHalf();
        state |= State::Thumb;
    }
    else
    {
        flushWord();
    }
}

template<u32 kInstr>
void Arm::Arm_BranchLink(u32 instr)
{
    constexpr uint kLink = bit::seq<24, 1>(kInstr);

    uint offset = bit::seq<0, 24>(instr);

    offset = bit::signEx<24>(offset);
    offset <<= 2;

    if (kLink)
        lr = pc - 4;

    pc += offset;
    flushWord();
}

template<u32 kInstr>
void Arm::Arm_DataProcessing(u32 instr)
{
    enum class Opcode
    {
        And, Eor, Sub, Rsb,
        Add, Adc, Sbc, Rsc,
        Tst, Teq, Cmp, Cmn,
        Orr, Mov, Bic, Mvn
    };

    constexpr uint kFlags  = bit::seq<20, 1>(kInstr);
    constexpr uint kOpcode = bit::seq<21, 4>(kInstr);
    constexpr uint kImmOp  = bit::seq<25, 1>(kInstr);

    uint rd    = bit::seq<12, 4>(instr);
    uint rn    = bit::seq<16, 4>(instr);
    uint flags = kFlags && rd != 15;

    u32& dst = regs[rd];
    u32  op1 = regs[rn];
    u32  op2;

    constexpr bool kLogical = 
           kOpcode == Opcode::And 
        || kOpcode == Opcode::Eor
        || kOpcode == Opcode::Orr
        || kOpcode == Opcode::Mov
        || kOpcode == Opcode::Bic
        || kOpcode == Opcode::Mvn
        || kOpcode == Opcode::Tst
        || kOpcode == Opcode::Teq;

    if (kImmOp)
    {
        uint value  = bit::seq<0, 8>(instr);
        uint amount = bit::seq<8, 4>(instr);
        op2 = ror<false>(value, amount << 1, flags && kLogical);
    }
    else
    {
        uint rm     = bit::seq<0, 4>(instr);
        uint reg_op = bit::seq<4, 1>(instr);
        uint shift  = bit::seq<5, 2>(instr);

        op2 = regs[rm];

        if (reg_op)
        {
            uint rs = bit::seq<8, 4>(instr);

            if (rn == 15) op1 += 4;
            if (rm == 15) op2 += 4;

            uint amount = regs[rs] & 0xFF;

            switch (Shift(shift))
            {
            case Shift::Lsl: op2 = lsl<false>(op2, amount, flags && kLogical); break;
            case Shift::Lsr: op2 = lsr<false>(op2, amount, flags && kLogical); break;
            case Shift::Asr: op2 = asr<false>(op2, amount, flags && kLogical); break;
            case Shift::Ror: op2 = ror<false>(op2, amount, flags && kLogical); break;

            default:
                SHELL_UNREACHABLE;
                break;
            }
            idle();
        }
        else
        {
            uint amount = bit::seq<7, 5>(instr);

            switch (Shift(shift))
            {
            case Shift::Lsl: op2 = lsl<true>(op2, amount, flags && kLogical); break;
            case Shift::Lsr: op2 = lsr<true>(op2, amount, flags && kLogical); break;
            case Shift::Asr: op2 = asr<true>(op2, amount, flags && kLogical); break;
            case Shift::Ror: op2 = ror<true>(op2, amount, flags && kLogical); break;

            default:
                SHELL_UNREACHABLE;
                break;
            }
        }
    }

    switch (Opcode(kOpcode))
    {
    case Opcode::And: dst = log(op1 &  op2, flags); break;
    case Opcode::Eor: dst = log(op1 ^  op2, flags); break;
    case Opcode::Orr: dst = log(op1 |  op2, flags); break;
    case Opcode::Mov: dst = log(       op2, flags); break;
    case Opcode::Bic: dst = log(op1 & ~op2, flags); break;
    case Opcode::Mvn: dst = log(      ~op2, flags); break;
    case Opcode::Tst:       log(op1 &  op2, true ); break;
    case Opcode::Teq:       log(op1 ^  op2, true ); break;
    case Opcode::Cmn:       add(op1,   op2, true ); break;
    case Opcode::Cmp:       sub(op1,   op2, true ); break;
    case Opcode::Add: dst = add(op1,   op2, flags); break;
    case Opcode::Adc: dst = adc(op1,   op2, flags); break;
    case Opcode::Sub: dst = sub(op1,   op2, flags); break;
    case Opcode::Sbc: dst = sbc(op1,   op2, flags); break;
    case Opcode::Rsb: dst = sub(op2,   op1, flags); break;
    case Opcode::Rsc: dst = sbc(op2,   op1, flags); break;

    default:
        SHELL_UNREACHABLE;
        break;
    }

    if (rd == 15)
    {
        if (kFlags)
        {
            Psr spsr = this->spsr;
            switchMode(spsr.m);
            cpsr = spsr;
        }

        constexpr bool kNoWriteback =
               kOpcode == Opcode::Cmp
            || kOpcode == Opcode::Cmn
            || kOpcode == Opcode::Tst
            || kOpcode == Opcode::Teq;

        if (!kNoWriteback)
        {
            if (cpsr.t)
            {
                flushHalf();
                state |= State::Thumb;
            }
            else
            {
                flushWord();
            }
        }
    }
}

template<u32 kInstr>
void Arm::Arm_StatusTransfer(u32 instr)
{
    enum class Bit
    {
        C = 1 << 16,
        X = 1 << 17,
        S = 1 << 18,
        F = 1 << 19
    };

    constexpr uint kWrite = bit::seq<21, 1>(kInstr);
    constexpr uint kSpsr  = bit::seq<22, 1>(kInstr);
    constexpr uint kImmOp = bit::seq<25, 1>(kInstr);

    if (kWrite)
    {
        u32 op;
        if (kImmOp)
        {
            uint value  = bit::seq<0, 8>(instr);
            uint amount = bit::seq<8, 4>(instr);
            op = bit::ror(value, amount << 1);
        }
        else
        {
            uint rm = bit::seq<0, 4>(instr);
            op = regs[rm];
        }

        u32 mask = 0;
        if (instr & Bit::C) mask |= 0x0000'00FF;
        if (instr & Bit::X) mask |= 0x0000'FF00;
        if (instr & Bit::S) mask |= 0x00FF'0000;
        if (instr & Bit::F) mask |= 0xFF00'0000;

        if (kSpsr)
        {
            spsr = (spsr & ~mask) | (op & mask);
        }
        else
        {
            if (instr & Bit::C)
                switchMode(Psr::Mode(op & 0x1F));

            cpsr = (cpsr & ~mask) | (op & mask);
        }
    }
    else
    {
        uint rd = bit::seq<12, 4>(instr);
        regs[rd] = kSpsr ? spsr : cpsr;
    }
}

template<u32 kInstr>
void Arm::Arm_Multiply(u32 instr)
{
    constexpr uint kFlags      = bit::seq<20, 1>(kInstr);
    constexpr uint kAccumulate = bit::seq<21, 1>(kInstr);

    uint rm = bit::seq< 0, 4>(instr);
    uint rs = bit::seq< 8, 4>(instr);
    uint rn = bit::seq<12, 4>(instr);
    uint rd = bit::seq<16, 4>(instr);

    u32  op1 = regs[rm];
    u32  op2 = regs[rs];
    u32  op3 = regs[rn];
    u32& dst = regs[rd];

    dst = op1 * op2;

    if (kAccumulate)
    {
        dst += op3;
        idle();
    }
    log(dst, kFlags);

    tickMultiply(op2, true);
}

template<u32 kInstr>
void Arm::Arm_MultiplyLong(u32 instr)
{
    constexpr uint kFlags      = bit::seq<20, 1>(kInstr);
    constexpr uint kAccumulate = bit::seq<21, 1>(kInstr);
    constexpr uint kSign       = bit::seq<22, 1>(kInstr);

    uint rm    = bit::seq< 0, 4>(instr);
    uint rs    = bit::seq< 8, 4>(instr);
    uint rd_lo = bit::seq<12, 4>(instr);
    uint rd_hi = bit::seq<16, 4>(instr);

    u64  op1    = regs[rm];
    u64  op2    = regs[rs];
    u32& dst_lo = regs[rd_lo];
    u32& dst_hi = regs[rd_hi];

    if (kSign)
    {
        op1 = bit::signEx<32>(op1);
        op2 = bit::signEx<32>(op2);
    }

    u64 res = op1 * op2;

    if (kAccumulate)
    {
        res += static_cast<u64>(dst_hi) << 32 | dst_lo;
        idle();
    }

    if (kFlags)
    {
        cpsr.z = res == 0;
        cpsr.n = bit::msb(res);
    }

    dst_lo = static_cast<u32>(res);
    dst_hi = static_cast<u32>(res >> 32);

    tickMultiply(op2, kSign);
    idle();
}

template<u32 kInstr>
void Arm::Arm_SingleDataTransfer(u32 instr)
{
    constexpr uint kLoad      = bit::seq<20, 1>(kInstr);
    constexpr uint kWriteback = bit::seq<21, 1>(kInstr);
    constexpr uint kByte      = bit::seq<22, 1>(kInstr);
    constexpr uint kIncrement = bit::seq<23, 1>(kInstr);
    constexpr uint kPreIndex  = bit::seq<24, 1>(kInstr);
    constexpr uint kRegOp     = bit::seq<25, 1>(kInstr);

    uint rd = bit::seq<12, 4>(instr);
    uint rn = bit::seq<16, 4>(instr);

    u32& dst = regs[rd];
    u32 addr = regs[rn];

    u32 offset;
    if (kRegOp)
    {
        uint rm     = bit::seq<0, 4>(instr);
        uint reg_op = bit::seq<4, 1>(instr);
        uint shift  = bit::seq<5, 2>(instr);

        uint amount;
        if (reg_op)
        {
            uint rs = bit::seq<8, 4>(instr);
            amount = regs[rs] & 0xFF;
        }
        else
        {
            amount = bit::seq<7, 5>(instr);
        }

        switch (Shift(shift))
        {
        case Shift::Lsl: offset = lsl<true>(regs[rm], amount, false); break;
        case Shift::Lsr: offset = lsr<true>(regs[rm], amount, false); break;
        case Shift::Asr: offset = asr<true>(regs[rm], amount, false); break;
        case Shift::Ror: offset = ror<true>(regs[rm], amount, false); break;

        default:
            SHELL_UNREACHABLE;
            break;
        }
    }
    else
    {
        offset = bit::seq<0, 12>(instr);
    }

    if (!kIncrement)
        offset = bit::twos(offset);

    if (kPreIndex)
        addr += offset;

    if (kLoad)
    {
        if (kByte)
            dst = readByte(addr);
        else
            dst = readWordRotate(addr);

        idle();

        if (rd == 15)
            flushWord();
    }
    else
    {
        u32 value = dst + (rd == 15 ? 4 : 0);

        if (kByte)
            writeByte(addr, value);
        else
            writeWord(addr, value);
    }

    if ((kWriteback || !kPreIndex) && (!kLoad || rd != rn))
    {
        if (!kPreIndex)
            addr += offset;

        regs[rn] = addr;
    }
}

template<u32 kInstr>
void Arm::Arm_HalfSignedDataTransfer(u32 instr)
{
    enum class Opcode { Swap, Ldrh, Ldrsb, Ldrsh };

    constexpr uint kOpcode    = bit::seq< 5, 2>(kInstr);
    constexpr uint kLoad      = bit::seq<20, 1>(kInstr);
    constexpr uint kWriteback = bit::seq<21, 1>(kInstr);
    constexpr uint kImmOp     = bit::seq<22, 1>(kInstr);
    constexpr uint kIncrement = bit::seq<23, 1>(kInstr);
    constexpr uint kPreIndex  = bit::seq<24, 1>(kInstr);

    static_assert(kOpcode != Opcode::Swap);

    uint rd = bit::seq<12, 4>(instr);
    uint rn = bit::seq<16, 4>(instr);

    u32& dst = regs[rd];
    u32 addr = regs[rn];

    u32 offset;
    if (kImmOp)
    {
        uint lower = bit::seq<0, 4>(instr);
        uint upper = bit::seq<8, 4>(instr);
        offset = (upper << 4) | lower;
    }
    else
    {
        uint rm = bit::seq<0, 4>(instr);
        offset = regs[rm];
    }

    if (!kIncrement)
        offset = bit::twos(offset);

    if (kPreIndex)
        addr += offset;

    if (kLoad)
    {
        switch (Opcode(kOpcode))
        {
        case Opcode::Ldrh:  dst = readHalfRotate(addr); break;
        case Opcode::Ldrsb: dst = readByteSignEx(addr); break;
        case Opcode::Ldrsh: dst = readHalfSignEx(addr); break;

        default:
            SHELL_UNREACHABLE;
            break;
        }

        idle();

        if (rd == 15)
            flushWord();
    }
    else
    {
        u32 value = dst + (rd == 15 ? 4 : 0);

        writeHalf(addr, value);
    }

    if ((kWriteback || !kPreIndex) && (!kLoad || rd != rn))
    {
        if (!kPreIndex)
            addr += offset;

        regs[rn] = addr;
    }
}

template<u32 kInstr>
void Arm::Arm_BlockDataTransfer(u32 instr)
{
    constexpr uint kLoad      = bit::seq<20, 1>(kInstr);
    constexpr uint kUserMode  = bit::seq<22, 1>(kInstr);
    constexpr uint kIncrement = bit::seq<23, 1>(kInstr);

    uint rlist     = bit::seq< 0, 16>(instr);
    uint rn        = bit::seq<16,  4>(instr);
    uint writeback = bit::seq<21,  1>(kInstr);
    uint pre_index = bit::seq<24,  1>(kInstr);

    u32 addr = regs[rn];
    u32 base = regs[rn];

    Psr::Mode mode;
    if (kUserMode)
    {
        mode = cpsr.m;
        switchMode(Psr::Mode::Usr);
    }

    if (rlist != 0)
    {
        if (!kIncrement)
        {
            addr -= 4 * bit::popcnt(rlist);
            pre_index ^= 0x1;

            if (writeback)
            {
                regs[rn] = addr;
                writeback = false;
            }
        }

        Access access = Access::NonSequential;

        if (kLoad)
        {
            if (rlist & (1 << rn))
                writeback = false;

            for (uint x : bit::iterate(rlist))
            {
                addr += 4 * pre_index;
                regs[x] = readWord(addr, access);
                addr += 4 * pre_index ^ 0x4;
                access = Access::Sequential;
            }

            idle();
            
            if (rlist & (1 << 15))
                flushWord();
        }
        else
        {
            for (uint x : bit::iterate(rlist))
            {
                u32 value = x != rn
                    ? x != 15
                        ? regs[x] + 0
                        : regs[x] + 4
                    : x == bit::ctz(rlist)
                        ? base
                        : base + (kIncrement ? 4 : -4) * bit::popcnt(rlist);

                addr += 4 * pre_index;
                writeWord(addr, value, access);
                addr += 4 * pre_index ^ 0x4;
                access = Access::Sequential;
            }
        }
    }
    else
    {
        if (kLoad)
        {
            pc = readWord(addr);
            flushWord();
        }
        else
        {
            enum class Suffix { DA, DB, IA, IB };

            switch (Suffix((kIncrement << 1) | pre_index))
            {
            case Suffix::DA: writeWord(addr - 0x3C, pc + 4); break;
            case Suffix::DB: writeWord(addr - 0x40, pc + 4); break;
            case Suffix::IA: writeWord(addr + 0x00, pc + 4); break;
            case Suffix::IB: writeWord(addr + 0x04, pc + 4); break;

            default:
                SHELL_UNREACHABLE;
                break;
            }
        }

        addr = kIncrement
            ? addr + 0x40
            : addr - 0x40;
    }

    if (writeback)
        regs[rn] = addr;

    if (kUserMode)
        switchMode(mode);
}

template<u32 kInstr>
void Arm::Arm_SingleDataSwap(u32 instr)
{
    constexpr uint kByte = bit::seq<22, 1>(kInstr);

    uint rm = bit::seq< 0, 4>(instr);
    uint rd = bit::seq<12, 4>(instr);
    uint rn = bit::seq<16, 4>(instr);

    u32  src = regs[rm];
    u32& dst = regs[rd];
    u32 addr = regs[rn];

    if (kByte)
    {
        dst = readByte(addr);
        writeByte(addr, src);
    }
    else
    {
        dst = readWordRotate(addr);
        writeWord(addr, src);
    }
    idle();
}

template<u32 kInstr>
void Arm::Arm_SoftwareInterrupt(u32 instr)
{
    interruptSw();
}

template<u32 kInstr>
void Arm::Arm_CoprocessorDataOperations(u32 instr)
{
    SHELL_ASSERT(false, SHELL_FUNCTION);
}

template<u32 kInstr>
void Arm::Arm_CoprocessorDataTransfers(u32 instr)
{
    SHELL_ASSERT(false, SHELL_FUNCTION);
}

template<u32 kInstr>
void Arm::Arm_CoprocessorRegisterTransfers(u32 instr)
{
    SHELL_ASSERT(false, SHELL_FUNCTION);
}

template<u32 kInstr>
void Arm::Arm_Undefined(u32 instr)
{
    SHELL_ASSERT(false, SHELL_FUNCTION);
}

template<uint kHash>
constexpr Arm::Instruction32 Arm::Arm_Decode()
{
    constexpr auto kDehash = dehashArm(kHash);
    constexpr auto kDecode = decodeArm(kHash);

    if constexpr (kDecode == InstructionArm::BranchExchange)               return &Arm::Arm_BranchExchange<kDehash>;
    if constexpr (kDecode == InstructionArm::BranchLink)                   return &Arm::Arm_BranchLink<kDehash>;
    if constexpr (kDecode == InstructionArm::DataProcessing)               return &Arm::Arm_DataProcessing<kDehash>;
    if constexpr (kDecode == InstructionArm::StatusTransfer)               return &Arm::Arm_StatusTransfer<kDehash>;
    if constexpr (kDecode == InstructionArm::Multiply)                     return &Arm::Arm_Multiply<kDehash>;
    if constexpr (kDecode == InstructionArm::MultiplyLong)                 return &Arm::Arm_MultiplyLong<kDehash>;
    if constexpr (kDecode == InstructionArm::SingleDataTransfer)           return &Arm::Arm_SingleDataTransfer<kDehash>;
    if constexpr (kDecode == InstructionArm::HalfSignedDataTransfer)       return &Arm::Arm_HalfSignedDataTransfer<kDehash>;
    if constexpr (kDecode == InstructionArm::BlockDataTransfer)            return &Arm::Arm_BlockDataTransfer<kDehash>;
    if constexpr (kDecode == InstructionArm::SingleDataSwap)               return &Arm::Arm_SingleDataSwap<kDehash>;
    if constexpr (kDecode == InstructionArm::SoftwareInterrupt)            return &Arm::Arm_SoftwareInterrupt<kDehash>;
    if constexpr (kDecode == InstructionArm::CoprocessorDataOperations)    return &Arm::Arm_CoprocessorDataOperations<kDehash>;
    if constexpr (kDecode == InstructionArm::CoprocessorDataTransfers)     return &Arm::Arm_CoprocessorDataTransfers<kDehash>;
    if constexpr (kDecode == InstructionArm::CoprocessorRegisterTransfers) return &Arm::Arm_CoprocessorRegisterTransfers<kDehash>;
    if constexpr (kDecode == InstructionArm::Undefined)                    return &Arm::Arm_Undefined<kDehash>;
}

#define DECODE0001(hash) Arm_Decode<hash>(),
#define DECODE0004(hash) DECODE0001(hash + 0 *    1) DECODE0001(hash + 1 *    1) DECODE0001(hash + 2 *    1) DECODE0001(hash + 3 *    1)
#define DECODE0016(hash) DECODE0004(hash + 0 *    4) DECODE0004(hash + 1 *    4) DECODE0004(hash + 2 *    4) DECODE0004(hash + 3 *    4)
#define DECODE0064(hash) DECODE0016(hash + 0 *   16) DECODE0016(hash + 1 *   16) DECODE0016(hash + 2 *   16) DECODE0016(hash + 3 *   16)
#define DECODE0256(hash) DECODE0064(hash + 0 *   64) DECODE0064(hash + 1 *   64) DECODE0064(hash + 2 *   64) DECODE0064(hash + 3 *   64)
#define DECODE1024(hash) DECODE0256(hash + 0 *  256) DECODE0256(hash + 1 *  256) DECODE0256(hash + 2 *  256) DECODE0256(hash + 3 *  256)
#define DECODE4096(hash) DECODE1024(hash + 0 * 1024) DECODE1024(hash + 1 * 1024) DECODE1024(hash + 2 * 1024) DECODE1024(hash + 3 * 1024)

const std::array<Arm::Instruction32, 4096> Arm::instr_arm = { DECODE4096(0) };

#undef DECODE0001
#undef DECODE0004
#undef DECODE0016
#undef DECODE0064
#undef DECODE0256
#undef DECODE1024
#undef DECODE4096
