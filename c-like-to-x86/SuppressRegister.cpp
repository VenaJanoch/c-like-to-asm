#include "SuppressRegister.h"

SuppressRegister::SuppressRegister(DosExeEmitter* emitter, CpuRegister reg)
    : emitter(emitter),
      reg(reg)
{
    emitter->suppressed_registers.insert(reg);
}

SuppressRegister::~SuppressRegister()
{
    emitter->suppressed_registers.erase(reg);
}