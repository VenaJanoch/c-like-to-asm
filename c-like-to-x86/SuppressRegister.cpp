#include "SuppressRegister.h"

SuppressRegister::SuppressRegister(DosExeEmitter* emitter, CpuRegister reg)
    : emitter(emitter),
      reg(reg)
{
    this->emitter->suppressed_registers.insert(reg);
}

SuppressRegister::~SuppressRegister()
{
    this->emitter->suppressed_registers.erase(reg);
}