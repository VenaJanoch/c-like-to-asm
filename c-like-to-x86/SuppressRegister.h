#pragma once

#include "DosExeEmitter.h"

class SuppressRegister
{
public:
    SuppressRegister(DosExeEmitter* emitter, i386::CpuRegister reg);
    ~SuppressRegister();

private:
    DosExeEmitter* emitter;
    i386::CpuRegister reg;
};