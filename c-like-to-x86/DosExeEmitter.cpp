#include "DosExeEmitter.h"

#include <iostream>
#include <list>
#include <memory>

#include "Compiler.h"
#include "CompilerException.h"
#include <functional>

DosExeEmitter::DosExeEmitter(Compiler* compiler)
    : compiler(compiler)
{
}

DosExeEmitter::~DosExeEmitter()
{
}

void DosExeEmitter::EmitMzHeader()
{
    int32_t header_size = sizeof(MzHeader);

    MzHeader* header = AllocateBuffer<MzHeader>();
    memset(header, 0, header_size);

    header->signature[0] = 'M';
    header->signature[1] = 'Z';
    header->header_paragraphs = (header_size >> 4) + 1;

    // ToDo: Set program size
    // ToDo: Create stack

    int32_t remaining = (header->header_paragraphs << 4) - header_size;
    if (remaining > 0) {
        uint8_t* fill = AllocateBuffer(remaining);
        memset(fill, 0, remaining);
    }
}

void DosExeEmitter::EmitInstructions(InstructionEntry* instruction_stream, SymbolTableEntry* symbol_table)
{
   
	Log("Compiling instructions...");

	CreateVariableList(symbol_table);

	InstructionEntry* current = instruction_stream;

	std::stack<InstructionEntry*> call_parameters;

    while (current) {
        

        InstructionEntry* next = current->next;

        SymbolTableEntry* symbol = symbol_table;

        switch (current->type) {
            case InstructionType::Assign: break;
			case InstructionType::Goto: EmitGoto(current); break;
			case InstructionType::GotoLabel: EmitGotoLabel(current); break;
            case InstructionType::If: break;
			case InstructionType::Push: EmitPush(current, call_parameters); break;
            case InstructionType::Call: break;
            case InstructionType::Return: break;

            default: ThrowOnUnreachableCode();
        }

        current = next;
    }
}

void DosExeEmitter::EmitSharedFunctions(SymbolTableEntry* symbol_table)
{

}

void DosExeEmitter::EmitStaticData()
{

}

void DosExeEmitter::CreateVariableList(SymbolTableEntry* symbol_table)
{
	SymbolTableEntry* current = symbol_table;

	while (current) {
		if (TypeIsValid(current->type)) {
			// Add all variables to list
			DosVariableDescriptor variable{};
			variable.symbol = current;
			variable.is_static = (current->parent == nullptr);
			variable.reg = CpuRegister::None;
			variable.location = 0;
			variables.push_back(variable);
		}

		current = current->next;
	}
}

void DosExeEmitter::Save(FILE* stream)
{
    if (buffer) {
        if (buffer_offset > 0) {
            if (!fwrite(buffer, buffer_offset, 1, stream)) {
                // ToDo: Something went wrong...
                Log("Emitting executable file failed");
            }
        }

        delete[] buffer;
        buffer = nullptr;
    }
}

void DosExeEmitter::SaveAndUnloadVariable(DosVariableDescriptor* var)
{
    int32_t var_size = compiler->GetSymbolTypeSize(var->symbol->type);
    switch (var_size) {
        case 1: {
            // Register to stack copy (8-bit range)
            uint8_t* a = AllocateBufferForInstruction(2 + 1);
            a[0] = 0x88;   // mov rm8, r8
            a[1] = ToXrm(1, var->reg, 6);
            a[2] = (int8_t)var->location;
            break;
        }
        case 2: {
            // Register to stack copy (8-bit range)
            uint8_t* a = AllocateBufferForInstruction(2 + 1);
            a[0] = 0x89;   // mov rm16/32, r16/32
            a[1] = ToXrm(1, var->reg, 6);
            a[2] = (int8_t)var->location;
            break;
        }
        case 4: {
            // Stack to register copy (8-bit range)
            uint8_t* a = AllocateBufferForInstruction(3 + 1);
            a[0] = 0x66;   // Operand size prefix
            a[1] = 0x89;   // mov rm16/32, r16/32
            a[2] = ToXrm(1, var->reg, 6);
            a[3] = (int8_t)var->location;
            break;
        }

        default: ThrowOnUnreachableCode();
    }

    var->reg = CpuRegister::None;
    var->is_dirty = false;
}

void DosExeEmitter::SaveAndUnloadRegister(CpuRegister reg)
{
    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->is_dirty && it->reg == reg && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            SaveAndUnloadVariable(&(*it));
            break;
        }

        ++it;
    }
}

void DosExeEmitter::SaveAndUnloadAllRegisters()
{
    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->is_dirty && it->reg != CpuRegister::None && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            SaveAndUnloadVariable(&(*it));
        }

        ++it;
    }
}

void DosExeEmitter::PushVariableToStack(DosVariableDescriptor* var, SymbolType param_type)
{
    int32_t param_size = compiler->GetSymbolTypeSize(param_type);

    if (var->reg != CpuRegister::None) {
        // Variable is already in register
        switch (param_size) {
            case 1: {
                // Zero high part of register and push it to parameter stack
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0x32;                       // xor r8, rm8
                a[1] = ToXrm(3, (uint8_t)var->reg + 4, (uint8_t)var->reg + 4);
                a[2] = 0x50 + (uint8_t)var->reg;   // push r16
                break;
            }
            case 2: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(1);
                a[0] = 0x50 + (uint8_t)var->reg;   // push r16
                break;
            }
            case 4: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(2);
                a[0] = 0x66;                       // Operand size prefix
                a[1] = 0x50 + (uint8_t)var->reg;   // push r32
                break;
            }

            default: ThrowOnUnreachableCode();
        }

        return;
    }

    // Variable is only on stack
    int32_t var_size = compiler->GetSymbolTypeSize(var->symbol->type);
    if (var_size < param_size) {
        // Variable expansion is needed
        CpuRegister reg_tmp = GetUnusedRegister();

        CopyVariableToRegister(var, reg_tmp, param_size);

        switch (param_size) {
            case 2: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(1);
                a[0] = 0x50 + (uint8_t)reg_tmp;    // push r16
                break;
            }
            case 4: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(2);
                a[0] = 0x66;                       // Operand size prefix
                a[1] = 0x50 + (uint8_t)reg_tmp;    // push r32
                break;
            }

            default: ThrowOnUnreachableCode();
        }

        return;
    }

    switch (param_size) {
        case 1: {
            if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack push using register (16-bit range)
                ThrowOnUnreachableCode();
            } else {
                // Stack push using register (8-bit range)
                CpuRegister reg_temp = GetUnusedRegister();

                uint8_t* a = AllocateBufferForInstruction(2 + 2 + 1 + 1);
                // Zero temp. register, because we have to push to stack full 16-bit word
                a[0] = 0x32;   // xor r8, rm8
                a[1] = ToXrm(3, (uint8_t)reg_temp + 4, (uint8_t)reg_temp + 4);

                a[2] = 0x8A;   // mov r8, rm8
                a[3] = ToXrm(1, reg_temp, 6);
                a[4] = (int8_t)var->location;

                a[5] = 0x50 + (uint8_t)reg_temp;   // push r16
            }
            break;
        }
        case 2: {
            if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack push (16-bit range)
                ThrowOnUnreachableCode();
            } else {
                // Stack push (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0xFF;   // push rm16
                a[1] = ToXrm(1, 6, 6);
                a[2] = (int8_t)var->location;
            }
            break;
        }
        case 4: {
            if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack push (16-bit range)
                ThrowOnUnreachableCode();
            } else {
                // Stack push (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                a[0] = 0x66;   // Operand size prefix
                a[1] = 0xFF;   // push rm32
                a[2] = ToXrm(1, 6, 6);
                a[3] = (int8_t)var->location;
            }
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

void DosExeEmitter::EmitGoto(InstructionEntry* i)
{
	// Cannot jump to itself, this should not happen,
	// because "goto" instructions are generated by compiler
	if (i->goto_statement.ip == ip_src) {
		ThrowOnUnreachableCode();
	}

	// Jumps to the next instruction are removed automatically as optimization
	if (i->goto_statement.ip == ip_src + 1) {
		return;
	}

	// Unload all registers before jump
	SaveAndUnloadAllRegisters();

	uint8_t* a = AllocateBufferForInstruction(3);
	a[0] = 0xE9; // jmp rel16

	if (i->goto_statement.ip < ip_src) {
		// ToDo: This can generate "jmp imm8" if possible
		uint32_t dst = ip_src_to_dst[i->goto_statement.ip];
		*(uint16_t*)(a + 1) = (int16_t)(dst - ip_dst);
		return;
	}

	// Create backpatch info, if the line was not precessed yet
	{
		DosBackpatchInstruction b{};
		b.type = DosBackpatchType::ToRel16;
		b.backpatch_offset = (a + 1) - buffer;
		b.backpatch_ip = ip_dst;
		b.target = DosBackpatchTarget::IP;
		b.ip_src = i->goto_statement.ip;
		backpatch.push_back(b);
	}
}

void DosExeEmitter::EmitGotoLabel(InstructionEntry* i)
{
	// Unload all registers before jump
	SaveAndUnloadAllRegisters();

	// Emit jump instruction
	uint8_t* a = AllocateBufferForInstruction(3);
	a[0] = 0xE9; // jmp rel16

	std::list<DosLabel>::iterator it = labels.begin();

	while (it != labels.end()) {
		if (strcmp(it->name, i->goto_label_statement.label) == 0) {
			*(uint16_t*)(a + 1) = (int16_t)(it->ip_dst - ip_dst);
			return;
		}

		++it;
	}


	// Create backpatch info, if the label was not defined yet
	{
		DosBackpatchInstruction b{};
		b.type = DosBackpatchType::ToRel16;
		b.backpatch_offset = (a + 1) - buffer;
		b.backpatch_ip = ip_dst;
		b.target = DosBackpatchTarget::Label;
		b.value = i->goto_label_statement.label;
		backpatch.push_back(b);
	}
}

void DosExeEmitter::EmitPush(InstructionEntry* i, std::stack<InstructionEntry*>& call_parameters)
{
	call_parameters.push(i);
}

uint8_t* DosExeEmitter::AllocateBuffer(uint32_t size)
{
    if (!buffer) {
        buffer_size = size + 256;
        buffer = new uint8_t[buffer_size];
        buffer_offset = size;
        return buffer;
    }

    if (buffer_size < buffer_offset + size) {
        buffer_size = buffer_offset + size + 64;
        buffer = (uint8_t*)realloc(buffer, buffer_size);
    }

    uint32_t prev_offset = buffer_offset;
    buffer_offset += size;
    return buffer + prev_offset;
}

uint8_t* DosExeEmitter::AllocateBuffer(uint32_t size)
{
    if (!buffer) {
        buffer_size = size + 256;
        buffer = (uint8_t*)malloc(buffer_size);
        buffer_offset = size;
        return buffer;
    }

    if (buffer_size < buffer_offset + size) {
        buffer_size = buffer_offset + size + 64;
        buffer = (uint8_t*)realloc(buffer, buffer_size);
    }

    uint32_t prev_offset = buffer_offset;
    buffer_offset += size;
    return buffer + prev_offset;
}

uint8_t* DosExeEmitter::AllocateBufferForInstruction(uint32_t size)
{
    ip_dst += size;
    return AllocateBuffer(size);
}

template<typename T>
T* DosExeEmitter::AllocateBuffer()
{
    return reinterpret_cast<T*>(AllocateBuffer(sizeof(T)));
}
