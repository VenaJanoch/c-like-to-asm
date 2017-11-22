#pragma once

#include "DosExeEmitter.h"

class SuppressRegister
{
public:
    SuppressRegister(DosExeEmitter* emitter, CpuRegister reg);
    ~SuppressRegister();

private:
    DosExeEmitter* emitter;
    CpuRegister reg;
};