#pragma once

#include "GenericEmitter.h"

enum struct CpuRegister {
    None = 0xFF,

    AL = 0,
    CL = 1,
    DL = 2,
    BL = 3,
    AH = 4,
    CH = 5,
    DH = 6,
    BH = 7,

    AX = 0,
    CX = 1,
    DX = 2,
    BX = 3,
    SP = 4,
    BP = 5,
    SI = 6,
    DI = 7
};

enum struct CpuSegment {
    ES = 0,
    CS = 1,
    SS = 2,
    DS = 3,
    FS = 4,
    GS = 5
};


#define ToOpR(op, r)    \
    (uint8_t)(op + ((uint8_t)(r) & 0x07))

#define ToXrm(x, r, m)  \
    (uint8_t)((((uint8_t)(x) << 6) & 0xC0) | (((uint8_t)(r) << 3) & 0x38) | ((uint8_t)(m) & 0x07))


/// <summary>
/// Class that emits machine code for i386 architecture
/// </summary>
class i386Emitter : protected GenericEmitter
{

protected:
    void AsmMov(CpuRegister to, CpuRegister from, int32_t size);
    void AsmMov(CpuRegister r16, CpuSegment sreg);
    void AsmMov(CpuSegment sreg, CpuRegister r16);

    void AsmAdd(CpuRegister to, CpuRegister from, int32_t size);
    void AsmSub(CpuRegister to, CpuRegister from, int32_t size);
    void AsmInc(CpuRegister r, int32_t size);
    void AsmDec(CpuRegister r, int32_t size);
    void AsmOr(CpuRegister to, CpuRegister from, int32_t size);

    void AsmProcEnter();
    void AsmProcLeave(uint16_t retn_imm16, bool restore_sp = false);
    void AsmProcLeaveNoArgs(uint16_t retn_imm16);

    void AsmInt(uint8_t imm8);
    void AsmInt(uint8_t imm8, uint8_t ah_imm8);

};