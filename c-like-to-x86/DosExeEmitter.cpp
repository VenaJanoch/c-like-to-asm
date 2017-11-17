#include "DosExeEmitter.h"

#include <iostream>
#include <list>
#include <memory>
#include <stack>
#include <functional>
#include <unordered_set>

#include "Compiler.h"
#include "CompilerException.h"

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
			case InstructionType::If: EmitIf(current); break;
			case InstructionType::Push: EmitPush(current, call_parameters); break;
			case InstructionType::Call: EmitCall(current, symbol_table, call_parameters); break;
			case InstructionType::Return: EmitReturn(current, symbol_table); break;

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

CpuRegister DosExeEmitter::GetUnusedRegister()
{
	// First four 32-registers are generally usable
	DosVariableDescriptor* register_used[4]{};

	std::list<DosVariableDescriptor>::iterator it = variables.begin();

	while (it != variables.end()) {
		if (it->reg != CpuRegister::None && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
			register_used[(int32_t)it->reg] = &(*it);
		}

		++it;
	}

	DosVariableDescriptor* last_used = nullptr;

	for (int32_t i = 0; i < 4; i++) {
		if (!register_used[i]) {
			// Register is empty (it was not used yet)
			return (CpuRegister)i;
		}

		if (!last_used || last_used->last_used > register_used[i]->last_used) {
			last_used = register_used[i];
		}
	}

	CpuRegister reg = last_used->reg;

	// Register was used, save it back to the stack and discard it
	if (last_used->is_dirty && last_used->reg != CpuRegister::None) {
		int32_t var_size = compiler->GetSymbolTypeSize(last_used->symbol->type);
		switch (var_size) {
		case 1: {
			// Register to stack copy (8-bit range)
			uint8_t* a = AllocateBufferForInstruction(2 + 1);
			a[0] = 0x88;   // mov rm8, r8
			a[1] = ToXrm(1, last_used->reg, 6);
			a[2] = (int8_t)last_used->location;
			break;
		}
		case 2: {
			// Register to stack copy (8-bit range)
			uint8_t* a = AllocateBufferForInstruction(2 + 1);
			a[0] = 0x89;   // mov rm16, r16
			a[1] = ToXrm(1, last_used->reg, 6);
			a[2] = (int8_t)last_used->location;
			break;
		}
		case 4: {
			// Stack to register copy (8-bit range)
			uint8_t* a = AllocateBufferForInstruction(3 + 1);
			a[0] = 0x66;   // Operand size prefix
			a[1] = 0x89;   // mov rm32, r32
			a[2] = ToXrm(1, last_used->reg, 6);
			a[3] = (int8_t)last_used->location;
			break;
		}

		default: ThrowOnUnreachableCode();
		}
	}

	last_used->reg = CpuRegister::None;
	last_used->is_dirty = false;

	return reg;
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

void DosExeEmitter::SaveVariable(DosVariableDescriptor* var)
{
    if (!var->is_dirty) {
        return;
    }

    if (!FindNextVariableReference(var)) {
        // Variable is not needed anymore, drop it...
        return;
    }

    if (var->location == 0) {
        ThrowOnUnreachableCode();
    }

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

    var->is_dirty = false;
}

DosVariableDescriptor* DosExeEmitter::FindVariableByName(char* name)
{
	// Search in function-local variables
	{
		std::list<DosVariableDescriptor>::iterator it = variables.begin();

		while (it != variables.end()) {
			if (it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0 && strcmp(it->symbol->name, name) == 0) {
				return &(*it);
			}

			++it;
		}
	}

	// Search in static (global) variables
	{
		std::list<DosVariableDescriptor>::iterator it = variables.begin();

		while (it != variables.end()) {
			if (!it->symbol->parent && strcmp(it->symbol->name, name) == 0) {
				return &(*it);
			}

			++it;
		}
	}

	ThrowOnUnreachableCode();
}

void DosExeEmitter::SaveAndUnloadRegister(CpuRegister reg)
{
    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->reg == reg && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            SaveVariable(&(*it));
            it->reg = CpuRegister::None;
            break;
        }

        ++it;
    }
}

void DosExeEmitter::SaveAndUnloadAllRegisters()
{
    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->reg != CpuRegister::None && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            SaveVariable(&(*it));
            it->reg = CpuRegister::None;
        }

        ++it;
    }
}

void DosExeEmitter::MarkRegisterAsDiscarded(CpuRegister reg)
{
    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->reg == reg && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            if (it->is_dirty) {
                ThrowOnUnreachableCode();
            }

            it->reg = CpuRegister::None;
            break;
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
                a[0] = 0x32;                    // xor r8, rm8
                a[1] = ToXrm(3, (uint8_t)var->reg + 4, (uint8_t)var->reg + 4);
                a[2] = ToOpR(0x50, var->reg);   // push r16
                break;
            }
            case 2: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(1);
                a[0] = ToOpR(0x50, var->reg);   // push r16
                break;
            }
            case 4: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(2);
                a[0] = 0x66;                    // Operand size prefix
                a[1] = ToOpR(0x50, var->reg);   // push r32
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
                a[0] = ToOpR(0x50, reg_tmp);    // push r16
                break;
            }
            case 4: {
                // Push register to parameter stack
                uint8_t* a = AllocateBufferForInstruction(2);
                a[0] = 0x66;                    // Operand size prefix
                a[1] = ToOpR(0x50, reg_tmp);    // push r32
                break;
            }

            default: ThrowOnUnreachableCode();
        }

        return;
    }

    switch (param_size) {
        case 1: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack push using register (16-bit range)
                ThrowOnTooFarCall();
            } else {
                // Stack push using register (8-bit range)
                CpuRegister reg_temp = GetUnusedRegister();

                uint8_t* a = AllocateBufferForInstruction(2 + 2 + 1 + 1);
                // Zero high/upper part of temp. register,
                // because we have to push to stack full 16-bit word
                a[0] = 0x32;                    // xor r8, rm8
                a[1] = ToXrm(3, (uint8_t)reg_temp + 4, (uint8_t)reg_temp + 4);

                a[2] = 0x8A;                    // mov r8, rm8
                a[3] = ToXrm(1, reg_temp, 6);
                a[4] = (int8_t)var->location;

                a[5] = ToOpR(0x50, reg_temp);   // push r16
            }
            break;
        }
        case 2: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack push (16-bit range)
                ThrowOnTooFarCall();
            } else {
                // Stack push (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0xFF;                // push rm16
                a[1] = ToXrm(1, 6, 6);
                a[2] = (int8_t)var->location;
            }
            break;
        }
        case 4: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack push (16-bit range)
                ThrowOnTooFarCall();
            } else {
                // Stack push (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                a[0] = 0x66;                // Operand size prefix
                a[1] = 0xFF;                // push rm32
                a[2] = ToXrm(1, 6, 6);
                a[3] = (int8_t)var->location;
            }
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

CpuRegister DosExeEmitter::LoadVariableUnreferenced(DosVariableDescriptor* var, int32_t desired_size)
{
    int32_t var_size = compiler->GetSymbolTypeSize(var->symbol->type);

    if (var->reg != CpuRegister::None && var_size >= desired_size) {
        // Variable is already loaded in register with desired size,
        // so only remove register ownership
        CpuRegister reg = var->reg;
        SaveVariable(var);
        var->reg = CpuRegister::None;
        return reg;
    }

    CpuRegister reg_dst;

    if (var_size < desired_size) {
        // Expansion is needed
        if (var->reg != CpuRegister::None) {
            // Variable is already in register with smaller size,
            // save it to stack and reload it with desired size
            reg_dst = var->reg;
            SaveVariable(var);
            var->reg = CpuRegister::None;
        } else {
            reg_dst = GetUnusedRegister();
        }

        ZeroRegister(reg_dst, desired_size);
    } else {
        reg_dst = GetUnusedRegister();
    }

    switch (var_size) {
        case 1: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack to register copy (16-bit range)
                ThrowOnUnreachableCode();
            } else {
                // Stack to register copy (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0x8A;   // mov r8, rm8
                a[1] = ToXrm(1, reg_dst, 6);
                a[2] = (int8_t)var->location;
            }
            break;
        }
        case 2: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack to register copy (16-bit range)
                ThrowOnUnreachableCode();
            } else {
                // Stack to register copy (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0x8B;   // mov r16, rm16
                a[1] = ToXrm(1, reg_dst, 6);
                a[2] = (int8_t)var->location;
            }
            break;
        }
        case 4: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack to register copy (16-bit range)
                ThrowOnUnreachableCode();
            } else {
                // Stack to register copy (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                a[0] = 0x66;   // Operand size prefix
                a[1] = 0x8B;   // mov r32, rm32
                a[2] = ToXrm(1, reg_dst, 6);
                a[3] = (int8_t)var->location;
            }
            break;
        }

        default: ThrowOnUnreachableCode();
    }

    return reg_dst;
}

void DosExeEmitter::CopyVariableToRegister(DosVariableDescriptor* var, CpuRegister reg_dst, int32_t desired_size)
{
    int32_t var_size = compiler->GetSymbolTypeSize(var->symbol->type);

    if (var->reg != CpuRegister::None && (var_size >= desired_size || var->reg != reg_dst)) {
        // Variable is already in desired register with desired size
        if (var->reg == reg_dst) {
            SaveVariable(var);
            var->reg = CpuRegister::None;
            return;
        }

        // Variable is in another register
        SaveAndUnloadRegister(reg_dst);

        if (var_size < desired_size) {
            ZeroRegister(reg_dst, desired_size);
        }

        // Copy value to desired register
        switch (var_size) {
            case 1: {
                uint8_t* a = AllocateBufferForInstruction(2);
                a[0] = 0x8A;   // mov r8, rm8
                a[1] = ToXrm(3, reg_dst, var->reg);
                break;
            }
            case 2: {
                uint8_t* a = AllocateBufferForInstruction(2);
                a[0] = 0x8B;   // mov r16, rm16
                a[1] = ToXrm(3, reg_dst, var->reg);
                break;
            }
            case 4: {
                uint8_t* a = AllocateBufferForInstruction(3);
                a[0] = 0x66;   // Operand size prefix
                a[1] = 0x8B;   // mov r32, rm32
                a[2] = ToXrm(3, reg_dst, var->reg);
                break;
            }

            default: ThrowOnUnreachableCode();
        }
        return;
    }

    // Variable is on the stack
    SaveAndUnloadRegister(reg_dst);

    if (var_size < desired_size) {
        ZeroRegister(reg_dst, desired_size);
    }

    switch (var_size) {
        case 1: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack to register copy (16-bit range)
                ThrowOnTooFarCall();
            } else {
                // Stack to register copy (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0x8A;   // mov r8, rm8
                a[1] = ToXrm(1, reg_dst, 6);
                a[2] = (int8_t)var->location;
            }
            break;
        }
        case 2: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack to register copy (16-bit range)
                ThrowOnTooFarCall();
            } else {
                // Stack to register copy (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                a[0] = 0x8B;   // mov r16, rm16
                a[1] = ToXrm(1, reg_dst, 6);
                a[2] = (int8_t)var->location;
            }
            break;
        }
        case 4: {
            if (var->location == 0) {
                ThrowOnUnreachableCode();
            } else if (var->location <= INT8_MIN || var->location >= INT8_MAX) {
                // Stack to register copy (16-bit range)
                ThrowOnTooFarCall();
            } else {
                // Stack to register copy (8-bit range)
                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                a[0] = 0x66;   // Operand size prefix
                a[1] = 0x8B;   // mov r32, rm32
                a[2] = ToXrm(1, reg_dst, 6);
                a[3] = (int8_t)var->location;
            }
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

void DosExeEmitter::LoadConstantToRegister(int32_t value, CpuRegister reg)
{
    MarkRegisterAsDiscarded(reg);

    if (value == (int8_t)value || value == (uint8_t)value) {
        uint8_t* a = AllocateBufferForInstruction(1 + 1);
        a[0] = ToOpR(0xB0, reg);    // mov r8, imm8
        *(uint8_t*)(a + 1) = (int8_t)value;
    } else if (value == (int16_t)value || value == (uint16_t)value) {
        uint8_t* a = AllocateBufferForInstruction(1 + 2);
        a[0] = ToOpR(0xB8, reg);     // mov r16, imm16
        *(uint16_t*)(a + 1) = (int16_t)value;
    } else {
        uint8_t* a = AllocateBufferForInstruction(2 + 4);
        a[0] = 0x66;                // Operand size prefix
        a[1] = ToOpR(0xB8, reg);    // mov r32, imm32
        *(uint32_t*)(a + 2) = value;
    }
}

void DosExeEmitter::LoadConstantToRegister(int32_t value, CpuRegister reg, int32_t desired_size)
{
    if (value == 0) {
        // If the value is zero, xor is faster
        ZeroRegister(reg, desired_size);
        return;
    }

    MarkRegisterAsDiscarded(reg);

    switch (desired_size) {
        case 1: {
            uint8_t* a = AllocateBufferForInstruction(1 + 1);
            a[0] = ToOpR(0xB0, reg);    // mov r8, imm8
            *(uint8_t*)(a + 1) = (int8_t)value;
            break;
        }
        case 2: {
            uint8_t* a = AllocateBufferForInstruction(1 + 2);
            a[0] = ToOpR(0xB8, reg);    // mov r16, imm16
            *(uint16_t*)(a + 1) = (int16_t)value;
            break;
        }
        case 4:
        case 8: {
            uint8_t* a = AllocateBufferForInstruction(2 + 4);
            a[0] = 0x66;                // Operand size prefix
            a[1] = ToOpR(0xB8, reg);    // mov r32, imm32
            *(uint32_t*)(a + 2) = value;
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

void DosExeEmitter::ZeroRegister(CpuRegister reg, int32_t desired_size)
{
    MarkRegisterAsDiscarded(reg);

    switch (desired_size) {
        case 1: {
            uint8_t* a = AllocateBufferForInstruction(2);
            a[0] = 0x32;   // xor r8, rm8
            a[1] = ToXrm(3, reg, reg);
            break;
        }

        case 2: {
            uint8_t* a = AllocateBufferForInstruction(2);
            a[0] = 0x33;   // xor r16, rm16
            a[1] = ToXrm(3, reg, reg);
            break;
        }

        case 4:
        case 8: {
            uint8_t* a = AllocateBufferForInstruction(3);
            a[0] = 0x66;   // Operand size prefix
            a[1] = 0x33;   // xor r32, rm32
            a[2] = ToXrm(3, reg, reg);
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

void DosExeEmitter::EmitEntryPointPrologue(SymbolTableEntry* function)
{
    parent = function;

    // Create new call frame
    uint8_t* a = AllocateBufferForInstruction(3);
    a[0] = 0x66;    // Operand size prefix
    a[1] = 0x8B;    // mov r32 (ebp), rm32 (esp)
    a[2] = ToXrm(3, CpuRegister::BP, CpuRegister::SP);

    // Allocate space for local variables
    int32_t stack_var_size = 0;

    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            if (!it->symbol->parameter && !it->symbol->is_temp) {
                stack_var_size += compiler->GetSymbolTypeSize(it->symbol->type);

                it->location = -stack_var_size;
            }
        }

        ++it;
    }

    if (stack_var_size > 0) {
        a = AllocateBufferForInstruction(2 + 2);
        a[0] = 0x66;    // Operand size prefix
        a[0] = 0x81;    // sub rm32 (esp), imm32 <size>
        a[1] = ToXrm(3, 5, CpuRegister::SP);
        *(uint16_t*)(a + 2) = stack_var_size;
    }

    // Clear function-local labels
    labels.clear();
}

void DosExeEmitter::EmitFunctionPrologue(SymbolTableEntry* function, SymbolTableEntry* symbol_table)
{
    parent = function;

    // Create backpatch information
    DosLabel label;
    label.name = function->name;
    label.ip_dst = ip_dst;
    functions.push_back(label);

    BackpatchLabels(label, DosBackpatchTarget::Function);

    // Create new call frame
    uint8_t* a = AllocateBufferForInstruction(2 + 3);
    a[0] = 0x66;                            // Operand size prefix
    a[1] = ToOpR(0x50, CpuRegister::BP);    // push ebp
    a[2] = 0x66;                            // Operand size prefix
    a[3] = 0x8B;                            // mov r32 (ebp), rm32 (esp)
    a[4] = ToXrm(3, CpuRegister::BP, CpuRegister::SP);

    // Allocate space for local variables in stack
    int32_t stack_var_size = 0;
    int32_t stack_param_size = 0;

    std::list<DosVariableDescriptor>::iterator it = variables.begin();

    while (it != variables.end()) {
        if (it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
            if (it->symbol->parameter) {
                int32_t size = compiler->GetSymbolTypeSize(it->symbol->type);
                if (size < 2) { // Min. push size is 2 bytes
                    size = 2;
                }

                // 4 bytes are used by ebp, 2 bytes are used for return address
                it->location = stack_param_size + 6;

                stack_param_size += size;
            } else if (!it->symbol->is_temp) {
                stack_var_size += compiler->GetSymbolTypeSize(it->symbol->type);

                it->location = -stack_var_size;
            }
        }

        ++it;
    }

    if (stack_var_size > 0) {
        a = AllocateBufferForInstruction(2 + 2);
        a[0] = 0x66;    // Operand size prefix
        a[0] = 0x81;    // sub rm32 (esp), imm32 <size>
        a[1] = ToXrm(3, 5, CpuRegister::SP);
        *(uint16_t*)(a + 2) = stack_var_size;
    }

    // Clear function-local labels
    labels.clear();
}

void DosExeEmitter::EmitAssign(InstructionEntry* i)
{
    switch (i->assignment.type) {
        case AssignType::None: {
            DosVariableDescriptor* dst = FindVariableByName(i->assignment.dst_value);

            switch (i->assignment.op1.exp_type) {
                case ExpressionType::Constant: {
                    if (i->assignment.op1.type == SymbolType::String) {
                        strings.insert(i->assignment.op1.value);

                        // Load string address to register
                        dst->reg = GetUnusedRegister();

                        uint8_t* a = AllocateBufferForInstruction(1 + 2);
                        a[0] = ToOpR(0xB8, dst->reg);   // mov r16, imm16

                                                        // Create backpatch info for string
                        {
                            DosBackpatchInstruction b{};
                            b.target = DosBackpatchTarget::String;
                            b.type = DosBackpatchType::ToDsAbs16;
                            b.backpatch_offset = (a + 1) - buffer;
                            b.value = i->assignment.op1.value;
                            backpatch.push_back(b);
                        }
                    } else {
                        //// No need to allocate register for constant value
                        //var->value = i->assignment.op1_value;

                        // Load constant to register
                        CpuRegister reg_dst = GetUnusedRegister();

                        int32_t value = atoi(i->assignment.op1.value);

                        int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);
                        LoadConstantToRegister(value, reg_dst, dst_size);

                        dst->reg = reg_dst;
                    }

                    dst->is_dirty = true;
                    dst->last_used = ip_src;
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->assignment.op1.value);

                    int32_t op1_size = compiler->GetSymbolTypeSize(op1->symbol->type);
                    int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);

                    if (op1->reg != CpuRegister::None && op1_size >= dst_size) {
                        // Variable with desired size is already in register,
                        // so only change register ownership
                        dst->reg = LoadVariableUnreferenced(op1, dst_size);
                        dst->is_dirty = true;
                        dst->last_used = ip_src;
                        break;
                    }

                    CpuRegister reg_dst = GetUnusedRegister();

                    if (op1->symbol->exp_type == ExpressionType::Constant) {
                        if (op1->symbol->type == SymbolType::String) {
                            ThrowOnUnreachableCode();
                            /*
                            // Load string address to register
                            reg_dst = GetUnusedRegister();

                            uint8_t* a = AllocateBufferForInstruction(1 + 2);
                            a[0] = ToOpR(0xB8, reg_dst);    // mov r16, imm16

                            // Create backpatch info for string
                            {
                            DosBackpatchInstruction b { };
                            b.target = DosBackpatchTarget::String;
                            b.type = DosBackpatchType::ToDsAbs16;
                            b.backpatch_offset = (a + 1) - buffer;
                            b.value = i->assignment.op1.value;
                            backpatch.push_back(b);
                            }
                            */
                        } else {
                            int32_t value = atoi(op1->value);
                            LoadConstantToRegister(value, reg_dst, dst_size);
                        }

                        dst->reg = reg_dst;
                        dst->is_dirty = true;
                        dst->last_used = ip_src;
                        break;
                    }

                    switch (dst_size) {
                        case 1: {
                            if (op1->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0x8A;   // mov r8, rm8
                                a[1] = ToXrm(3, reg_dst, op1->reg);
                            } else if (op1->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op1->location <= INT8_MIN || op1->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0x8A;   // mov r8, rm8
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op1->location;
                            }
                            break;
                        }
                        case 2: {
                            if (op1->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0x8B;   // mov r16, rm16
                                a[1] = ToXrm(3, reg_dst, op1->reg);
                            } else if (op1->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op1->location <= INT8_MIN || op1->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0x8B;   // mov r16, rm16
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op1->location;
                            }
                            break;
                        }
                        case 4: {
                            if (op1->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(3);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = 0x8B;   // mov r32, rm32
                                a[2] = ToXrm(3, reg_dst, op1->reg);
                            } else if (op1->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op1->location <= INT8_MIN || op1->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = 0x8B;   // mov r32, rm32
                                a[2] = ToXrm(1, reg_dst, 6);
                                a[3] = (int8_t)op1->location;
                            }
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }

                    dst->reg = reg_dst;
                    dst->is_dirty = true;
                    dst->last_used = ip_src;
                    break;
                }

                default: ThrowOnUnreachableCode();
            }
            break;
        }

        case AssignType::Negation: {
            DosVariableDescriptor* dst = FindVariableByName(i->assignment.dst_value);

            // ToDo
            /*if (dst->symbol->exp_type == ExpressionType::Constant) {
            size_t str_length = strlen(i->assignment.op1.value);
            if (i->assignment.op1.value[0] == '-') {
            char* value_neg = new char[str_length];
            memcpy(value_neg, i->assignment.op1.value + 1, str_length - 1);
            value_neg[str_length - 1] = '\0';
            dst->value = value_neg;
            } else {
            char* value_neg = new char[str_length + 2];
            value_neg[0] = '-';
            memcpy(value_neg + 1, i->assignment.op1.value, str_length);
            value_neg[str_length + 1] = '\0';
            dst->value = value_neg;
            }
            break;
            }*/

            CpuRegister reg_dst = dst->reg;
            if (reg_dst == CpuRegister::None) {
                reg_dst = GetUnusedRegister();
            }

            int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);

            switch (i->assignment.op1.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op1.value);
                    LoadConstantToRegister(value, reg_dst, dst_size);
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->assignment.op1.value);
                    CopyVariableToRegister(op1, reg_dst, dst_size);
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            switch (dst_size) {
                case 1: {
                    uint8_t* a = AllocateBufferForInstruction(2);
                    a[0] = 0xF6;   // neg rm8
                    a[1] = ToXrm(3, 3, reg_dst);
                    break;
                }
                case 2: {
                    uint8_t* a = AllocateBufferForInstruction(2);
                    a[0] = 0xF7;   // neg rm16
                    a[1] = ToXrm(3, 3, reg_dst);
                    break;
                }
                case 4: {
                    uint8_t* a = AllocateBufferForInstruction(3);
                    a[0] = 0x66;   // Operand size prefix
                    a[1] = 0xF7;   // neg rm32
                    a[2] = ToXrm(1, 3, reg_dst);
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            dst->reg = reg_dst;
            dst->is_dirty = true;
            dst->last_used = ip_src;
            break;
        }

        case AssignType::Add:
        case AssignType::Subtract: {
            DosVariableDescriptor* dst = FindVariableByName(i->assignment.dst_value);

            if (i->assignment.type == AssignType::Add && dst->symbol->type == SymbolType::String) {
                if (i->assignment.op1.exp_type == ExpressionType::Constant && i->assignment.op2.exp_type == ExpressionType::Constant) {
                    size_t length1 = strlen(i->assignment.op1.value);
                    size_t length2 = strlen(i->assignment.op2.value);

                    char* concat = new char[length1 + length2 + 1];
                    memcpy(concat, i->assignment.op1.value, length1);
                    memcpy(concat + length1, i->assignment.op2.value, length2);
                    concat[length1 + length2] = '\0';

                    strings.insert(concat);

                    dst->value = concat;
                    //dst->symbol->exp_type = ExpressionType::Constant;

                    // Load string address to register
                    dst->reg = GetUnusedRegister();

                    uint8_t* a = AllocateBufferForInstruction(1 + 2);
                    a[0] = ToOpR(0xB8, dst->reg);   // mov r16, imm16

                                                    // Create backpatch info for string
                    {
                        DosBackpatchInstruction b{};
                        b.target = DosBackpatchTarget::String;
                        b.type = DosBackpatchType::ToDsAbs16;
                        b.backpatch_offset = (a + 1) - buffer;
                        b.value = concat;
                        backpatch.push_back(b);
                    }
                } else {
                    ThrowOnUnreachableCode();
                }

                dst->is_dirty = true;
                dst->last_used = ip_src;
                return;
            }

            bool constant_swapped = false;

            if (i->assignment.op1.exp_type == ExpressionType::Constant) {
                // Constant has to be second operand, swap them
                std::swap(i->assignment.op1, i->assignment.op2);

                constant_swapped = true;
            }

            int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);

            if (i->assignment.op1.exp_type == ExpressionType::Constant) {
                // Both operands are constants
                int32_t value1 = atoi(i->assignment.op1.value);
                int32_t value2 = atoi(i->assignment.op2.value);

                if (i->assignment.type == AssignType::Add) {
                    value1 += value2;
                } else {
                    value1 -= value2;
                }

                CpuRegister reg_dst = GetUnusedRegister();

                LoadConstantToRegister(value1, reg_dst, dst_size);

                dst->reg = reg_dst;
                dst->is_dirty = true;
                dst->last_used = ip_src;
                return;
            }

            DosVariableDescriptor* op1 = FindVariableByName(i->assignment.op1.value);

            CpuRegister reg_dst;
            if (dst == op1 && op1->reg != CpuRegister::None) {
                reg_dst = op1->reg;
            } else {
                reg_dst = LoadVariableUnreferenced(op1, dst_size);
            }

            switch (i->assignment.op2.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op2.value);
                    if (i->assignment.type == AssignType::Subtract) {
                        value = -value;
                    }

                    uint8_t* a = AllocateBufferForInstruction(3 + 4);
                    a[0] = 0x66;        // Operand size prefix
                    a[1] = 0x81;        // add rm32, imm32
                    a[2] = ToXrm(3, 0, reg_dst);
                    *(int32_t*)(a + 3) = value;

                    if (i->assignment.type == AssignType::Subtract && constant_swapped) {
                        uint8_t* neg = AllocateBufferForInstruction(3);
                        neg[0] = 0x66;  // Operand size prefix
                        neg[1] = 0xF7;  // neg rm32
                        neg[2] = ToXrm(3, 3, reg_dst);
                    }
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op2 = FindVariableByName(i->assignment.op2.value);

                    switch (dst_size) {
                        case 1: {
                            uint8_t opcode = (i->assignment.type == AssignType::Add ? 0x02 : 0x2A);
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = opcode; // add/sub r8, rm8
                                a[1] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = opcode; // add/sub r8, rm8
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 2: {
                            uint8_t opcode = (i->assignment.type == AssignType::Add ? 0x03 : 0x2B);
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = opcode; // add/sub r16, rm16
                                a[1] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = opcode; // add/sub r16, rm16
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 4: {
                            uint8_t opcode = (i->assignment.type == AssignType::Add ? 0x03 : 0x2B);
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(3);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = opcode; // add/sub r32, rm32
                                a[2] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = opcode; // add/sub r32, rm32
                                a[2] = ToXrm(1, reg_dst, 6);
                                a[3] = (int8_t)op2->location;
                            }
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            dst->reg = reg_dst;
            dst->is_dirty = true;
            dst->last_used = ip_src;
            break;
        }

        case AssignType::Multiply: {
            DosVariableDescriptor* dst = FindVariableByName(i->assignment.dst_value);

            int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);

            if (i->assignment.op1.exp_type == ExpressionType::Constant) {
                // Constant has to be second operand, swap them
                std::swap(i->assignment.op1, i->assignment.op2);
            }

            if (i->assignment.op1.exp_type == ExpressionType::Constant) {
                // Both operands are constants - constant expression
                int32_t value1 = atoi(i->assignment.op1.value);
                int32_t value2 = atoi(i->assignment.op2.value);

                value1 *= value2;

                CpuRegister reg_dst = GetUnusedRegister();

                LoadConstantToRegister(value1, reg_dst, dst_size);

                dst->reg = reg_dst;
                dst->is_dirty = true;
                dst->last_used = ip_src;
                return;
            }

            DosVariableDescriptor* op1 = FindVariableByName(i->assignment.op1.value);

            switch (i->assignment.op2.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op2.value);

                    SaveAndUnloadRegister(CpuRegister::AX);
                    LoadConstantToRegister(value, CpuRegister::AX, dst_size);

                    switch (dst_size) {
                        case 1: {
                            if (op1->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0xF6;   // mul r8, rm8
                                a[1] = ToXrm(3, 4, op1->reg);
                            } else if (op1->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op1->location <= INT8_MIN || op1->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0xF6;   // mul r8, rm8
                                a[1] = ToXrm(1, 4, 6);
                                a[2] = (int8_t)op1->location;
                            }
                            break;
                        }
                        case 2: {
                            // DX register will be discarted after multiply
                            SaveAndUnloadRegister(CpuRegister::DX);

                            if (op1->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0xF7;   // mul r16, rm16
                                a[1] = ToXrm(3, 4, op1->reg);
                            } else if (op1->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op1->location <= INT8_MIN || op1->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0xF7;   // mul r16, rm16
                                a[1] = ToXrm(1, 4, 6);
                                a[2] = (int8_t)op1->location;
                            }
                            break;
                        }
                        case 4: {
                            // DX register will be discarted after multiply
                            SaveAndUnloadRegister(CpuRegister::DX);

                            if (op1->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(3);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = 0xF7;   // mul r32, rm32
                                a[2] = ToXrm(3, 4, op1->reg);
                            } else if (op1->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op1->location <= INT8_MIN || op1->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = 0xF7;   // mul r32, rm32
                                a[2] = ToXrm(1, 4, 6);
                                a[3] = (int8_t)op1->location;
                            }
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op2 = FindVariableByName(i->assignment.op2.value);

                    if (op2->reg == CpuRegister::AX) {
                        // If the second operand is already in AX, swap them
                        // If not, it doesn't matter, one operand has to be in AX
                        std::swap(op1, op2);
                    }

                    CopyVariableToRegister(op1, CpuRegister::AX, dst_size);

                    // Have to set it here, because it is overwritten by "op2" sometimes
                    dst->reg = CpuRegister::AX;
                    dst->is_dirty = true;
                    dst->last_used = ip_src;

                    int32_t op2_size = compiler->GetSymbolTypeSize(op2->symbol->type);
                    if (op2_size < dst_size) {
                        // Required size is higher than provided, unreference and expand it
                        op2->reg = LoadVariableUnreferenced(op2, dst_size);
                    }

                    switch (dst_size) {
                        case 1: {
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0xF6;   // mul r8, rm8
                                a[1] = ToXrm(3, 4, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0xF6;   // mul r8, rm8
                                a[1] = ToXrm(1, 4, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 2: {
                            // DX register will be discarted after multiply
                            SaveAndUnloadRegister(CpuRegister::DX);

                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0xF7;   // mul r16, rm16
                                a[1] = ToXrm(3, 4, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0xF7;   // mul r16, rm16
                                a[1] = ToXrm(1, 4, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 4: {
                            // DX register will be discarted after multiply
                            SaveAndUnloadRegister(CpuRegister::DX);

                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(3);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = 0xF7;   // mul r32, rm32
                                a[2] = ToXrm(3, 4, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = 0xF7;   // mul r32, rm32
                                a[2] = ToXrm(1, 4, 6);
                                a[3] = (int8_t)op2->location;
                            }
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            dst->reg = CpuRegister::AX;
            dst->is_dirty = true;
            dst->last_used = ip_src;
            break;
        }

        case AssignType::Divide:
        case AssignType::Remainder: {
            DosVariableDescriptor* dst = FindVariableByName(i->assignment.dst_value);

            int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);

            switch (i->assignment.op1.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op1.value);

                    SaveAndUnloadRegister(CpuRegister::AX);
                    // Load with higher size than destination to clear upper/high part
                    // of the register, so the register will be ready for division
                    LoadConstantToRegister(value, CpuRegister::AX, dst_size * 2);
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->assignment.op1.value);

                    // Copy with higher size than destination to clear upper/high part
                    // of the register, so the register will be ready for division
                    CopyVariableToRegister(op1, CpuRegister::AX, dst_size * 2);
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            // Have to set it here, because it is overwritten by "op2" sometimes
            dst->reg = CpuRegister::AX;
            dst->is_dirty = true;
            dst->last_used = ip_src;

            DosVariableDescriptor* op2 = nullptr;
            CpuRegister op2_reg;
            switch (i->assignment.op2.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op2.value);

                    //SaveAndUnloadRegister(CpuRegister::CX);
                    //LoadConstantToRegister(value, CpuRegister::CX, dst_size);
                    //op2_reg = CpuRegister::CX;

                    op2_reg = GetUnusedRegister();
                    LoadConstantToRegister(value, op2_reg, dst_size);
                    break;
                }
                case ExpressionType::Variable: {
                    op2 = FindVariableByName(i->assignment.op2.value);
                    op2_reg = op2->reg;
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            switch (dst_size) {
                case 1: {
                    if (op2_reg != CpuRegister::None) {
                        // Register to register copy
                        uint8_t* a = AllocateBufferForInstruction(2);
                        a[0] = 0xF6;   // div r8, rm8
                        a[1] = ToXrm(3, 6, op2_reg);
                    } else if (op2->location == 0) {
                        ThrowOnUnreachableCode();
                    } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                        // Stack to register copy (16-bit range)
                        ThrowOnTooFarCall();
                    } else {
                        // Stack to register copy (8-bit range)
                        uint8_t* a = AllocateBufferForInstruction(2 + 1);
                        a[0] = 0xF6;   // div r8, rm8
                        a[1] = ToXrm(1, 6, 6);
                        a[2] = (int8_t)op2->location;
                    }

                    if (i->assignment.type == AssignType::Remainder) {
                        // Copy remainder from AH to AL
                        uint8_t* a = AllocateBufferForInstruction(2);
                        a[0] = 0x8A;   // mov r8, rm8
                        a[1] = ToXrm(3, CpuRegister::AL, CpuRegister::AH);
                    }

                    ZeroRegister(CpuRegister::AH, 1);

                    //dst->reg = CpuRegister::AX;
                    break;
                }
                case 2: {
                    // DX register will be discarted after multiply
                    SaveAndUnloadRegister(CpuRegister::DX);

                    ZeroRegister(CpuRegister::DX, 2);

                    if (op2_reg != CpuRegister::None) {
                        // Register to register copy
                        uint8_t* a = AllocateBufferForInstruction(2);
                        a[0] = 0xF7;   // div r16, rm16
                        a[1] = ToXrm(3, 6, op2_reg);
                    } else if (op2->location == 0) {
                        ThrowOnUnreachableCode();
                    } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                        // Stack to register copy (16-bit range)
                        ThrowOnTooFarCall();
                    } else {
                        // Stack to register copy (8-bit range)
                        uint8_t* a = AllocateBufferForInstruction(2 + 1);
                        a[0] = 0xF7;   // div r16, rm16
                        a[1] = ToXrm(1, 6, 6);
                        a[2] = (int8_t)op2->location;
                    }

                    dst->reg = (i->assignment.type == AssignType::Remainder ? CpuRegister::DX : CpuRegister::AX);
                    break;
                }
                case 4: {
                    // DX register will be discarted after multiply
                    SaveAndUnloadRegister(CpuRegister::DX);

                    ZeroRegister(CpuRegister::DX, 4);

                    if (op2_reg != CpuRegister::None) {
                        // Register to register copy
                        uint8_t* a = AllocateBufferForInstruction(3);
                        a[0] = 0x66;   // Operand size prefix
                        a[1] = 0xF7;   // div r32, rm32
                        a[2] = ToXrm(3, 6, op2_reg);
                    } else if (op2->location == 0) {
                        ThrowOnUnreachableCode();
                    } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                        // Stack to register copy (16-bit range)
                        ThrowOnTooFarCall();
                    } else {
                        // Stack to register copy (8-bit range)
                        uint8_t* a = AllocateBufferForInstruction(3 + 1);
                        a[0] = 0x66;   // Operand size prefix
                        a[1] = 0xF7;   // div r32, rm32
                        a[2] = ToXrm(1, 6, 6);
                        a[3] = (int8_t)op2->location;
                    }

                    dst->reg = (i->assignment.type == AssignType::Remainder ? CpuRegister::DX : CpuRegister::AX);
                    break;
                }

                default: ThrowOnUnreachableCode();
            }
            break;
        }

        case AssignType::ShiftLeft:
        case AssignType::ShiftRight: {
            DosVariableDescriptor* dst = FindVariableByName(i->assignment.dst_value);

            int32_t dst_size = compiler->GetSymbolTypeSize(dst->symbol->type);

            switch (i->assignment.op2.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op2.value);

                    SaveAndUnloadRegister(CpuRegister::CL);
                    LoadConstantToRegister(value, CpuRegister::CL, dst_size);
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op2 = FindVariableByName(i->assignment.op2.value);

                    CopyVariableToRegister(op2, CpuRegister::CL, dst_size);
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            // Have to set it here, because it is overwritten by "op1" sometimes
            dst->reg = CpuRegister::CL;
            dst->is_dirty = true;
            dst->last_used = ip_src;

            CpuRegister reg_dst;
            switch (i->assignment.op1.exp_type) {
                case ExpressionType::Constant: {
                    int32_t value = atoi(i->assignment.op1.value);

                    reg_dst = GetUnusedRegister();
                    LoadConstantToRegister(value, reg_dst, dst_size);
                    break;
                }
                case ExpressionType::Variable: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->assignment.op1.value);
                    int32_t op1_size = compiler->GetSymbolTypeSize(dst->symbol->type);

                    if (dst == op1 && op1->reg != CpuRegister::None && dst_size <= op1_size) {
                        reg_dst = op1->reg;
                    } else {
                        reg_dst = LoadVariableUnreferenced(op1, dst_size);
                    }
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            uint8_t type = (i->assignment.type == AssignType::ShiftLeft ? 4 : 5);

            switch (dst_size) {
                case 1: {
                    uint8_t* a = AllocateBufferForInstruction(2);
                    a[0] = 0xD2;    // shl/shr rm8, cl
                    a[1] = ToXrm(3, type, reg_dst);
                    break;
                }
                case 2: {
                    uint8_t* a = AllocateBufferForInstruction(2);
                    a[0] = 0xD3;    // shl/shr rm16, cl
                    a[1] = ToXrm(3, type, reg_dst);
                    break;
                }
                case 4: {
                    uint8_t* a = AllocateBufferForInstruction(3);
                    a[0] = 0xD3;    // Operand size prefix
                    a[1] = 0xF6;    // shl/shr rm32, cl
                    a[2] = ToXrm(3, type, reg_dst);
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            dst->reg = reg_dst;
            dst->is_dirty = true;
            dst->last_used = ip_src;
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

    uint8_t* a = AllocateBufferForInstruction(1 + 2);
    a[0] = 0xE9;    // jmp rel16

    if (i->goto_statement.ip < ip_src) {
        // ToDo: This can generate "jmp rel8" if possible
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

void DosExeEmitter::EmitIf(InstructionEntry* i)
{
    // Unload all registers before jump
    SaveAndUnloadAllRegisters();

    uint8_t* goto_ptr = nullptr;

    if (i->if_statement.op1.exp_type == ExpressionType::Constant) {
        // Constant has to be second operand, swap them
        std::swap(i->if_statement.op1, i->if_statement.op2);

        i->if_statement.type = GetSwappedCompareType(i->if_statement.type);
    }

    if (i->if_statement.op1.exp_type == ExpressionType::Constant) {
        // Both operands are constants
        int32_t value1 = atoi(i->if_statement.op1.value);
        int32_t value2 = atoi(i->if_statement.op2.value);

        if (IfConstexpr(i->if_statement.type, value1, value2)) {
            uint8_t* a = AllocateBufferForInstruction(1 + 1);
            a[0] = 0xEB;   // jmp rel8

            goto_ptr = a + 1;
            goto DoBackpatch;
        } else {
            return;
        }
    }

    switch (i->if_statement.type) {
        case CompareType::LogOr:
        case CompareType::LogAnd: {
            switch (i->if_statement.op2.exp_type) {
                case ExpressionType::Constant: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->if_statement.op1.value);

                    int32_t op1_size = compiler->GetSymbolTypeSize(op1->symbol->type);

                    int32_t value = atoi(i->if_statement.op2.value);

                    CpuRegister reg_dst = LoadVariableUnreferenced(op1, op1_size);

                    uint8_t type = (i->if_statement.type == CompareType::LogOr ? 1 : 0);

                    // ToDo: This should be max(op1_size, op2_size)
                    switch (op1_size) {
                        case 1: {
                            uint8_t* a = AllocateBufferForInstruction(2 + 1);
                            a[0] = 0x80;   // or/and rm8, imm8
                            a[1] = ToXrm(3, type, reg_dst);
                            *(uint8_t*)(a + 2) = (int8_t)value;
                            break;
                        }
                        case 2: {
                            uint8_t* a = AllocateBufferForInstruction(2 + 2);
                            a[0] = 0x81;   // or/and rm16, imm16
                            a[1] = ToXrm(3, type, reg_dst);
                            *(uint16_t*)(a + 2) = (int16_t)value;
                            break;
                        }
                        case 4: {
                            uint8_t* a = AllocateBufferForInstruction(3 + 4);
                            a[0] = 0x66;   // Operand size prefix
                            a[1] = 0x81;   // or/and rm32, imm32
                            a[2] = ToXrm(3, type, reg_dst);
                            *(uint32_t*)(a + 3) = (int32_t)value;
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }

                case ExpressionType::Variable: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->if_statement.op1.value);
                    DosVariableDescriptor* op2 = FindVariableByName(i->if_statement.op2.value);

                    if (op2->reg != CpuRegister::None) {
                        // If the second operand is already in register, swap them.
                        // If not, it doesn't matter, one operand has to be in register.
                        std::swap(op1, op2);
                    }

                    int32_t op1_size = compiler->GetSymbolTypeSize(op1->symbol->type);

                    CpuRegister reg_dst = LoadVariableUnreferenced(op1, op1_size);

                    // ToDo: This should be max(op1_size, op2_size)
                    switch (op1_size) {
                        case 1: {
                            uint8_t opcode = (i->if_statement.type == CompareType::LogOr ? 0x0A : 0x22);
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = opcode; // or/and r8, rm8
                                a[1] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = opcode; // or/and r8, rm8
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 2: {
                            uint8_t opcode = (i->if_statement.type == CompareType::LogOr ? 0x0B : 0x23);
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = opcode; // or/and r16, rm16
                                a[1] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = opcode; // or/and r16, rm16
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 4: {
                            uint8_t opcode = (i->if_statement.type == CompareType::LogOr ? 0x0B : 0x23);
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(3);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = opcode; // or/and r32, rm32
                                a[2] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                                a[0] = 0x66;   // Operand size prefix
                                a[1] = opcode; // or/and r32, rm32
                                a[2] = ToXrm(1, reg_dst, 6);
                                a[3] = (int8_t)op2->location;
                            }
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            uint8_t* a = AllocateBufferForInstruction(1 + 1);
            a[0] = 0x75; // jnz rel8

            goto_ptr = a + 1;
            break;
        }

        case CompareType::Equal:
        case CompareType::NotEqual:
        case CompareType::Greater:
        case CompareType::Less:
        case CompareType::GreaterOrEqual:
        case CompareType::LessOrEqual: {
            switch (i->if_statement.op2.exp_type) {
                case ExpressionType::Constant: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->if_statement.op1.value);
                    int32_t op1_size = compiler->GetSymbolTypeSize(op1->symbol->type);

                    int32_t value = atoi(i->if_statement.op2.value);

                    CpuRegister reg_dst = LoadVariableUnreferenced(op1, op1_size);

                    // ToDo: This should be max(op1_size, op2_size)
                    switch (op1_size) {
                        case 1: {
                            uint8_t* a = AllocateBufferForInstruction(2 + 1);
                            a[0] = 0x80;    // cmp rm8, imm8
                            a[1] = ToXrm(3, 7, reg_dst);
                            *(uint8_t*)(a + 2) = (int8_t)value;
                            break;
                        }
                        case 2: {
                            uint8_t* a = AllocateBufferForInstruction(2 + 2);
                            a[0] = 0x81;    // cmp rm16, imm16
                            a[1] = ToXrm(3, 7, reg_dst);
                            *(uint16_t*)(a + 2) = (int16_t)value;
                            break;
                        }
                        case 4: {
                            uint8_t* a = AllocateBufferForInstruction(3 + 4);
                            a[0] = 0x66;    // Operand size prefix
                            a[1] = 0x81;    // cmp rm32, imm32
                            a[2] = ToXrm(3, 7, reg_dst);
                            *(uint32_t*)(a + 3) = (int32_t)value;
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }

                case ExpressionType::Variable: {
                    DosVariableDescriptor* op1 = FindVariableByName(i->if_statement.op1.value);
                    DosVariableDescriptor* op2 = FindVariableByName(i->if_statement.op2.value);

                    if (op2->reg != CpuRegister::None) {
                        // If the second operand is already in register, swap them
                        // If not, it doesn't matter, one operand has to be in register
                        std::swap(op1, op2);

                        i->if_statement.type = GetSwappedCompareType(i->if_statement.type);
                    }

                    int32_t op1_size = compiler->GetSymbolTypeSize(op1->symbol->type);

                    CpuRegister reg_dst = LoadVariableUnreferenced(op1, op1_size);

                    // ToDo: This should be max(op1_size, op2_size)
                    switch (op1_size) {
                        case 1: {
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0x3A;    // cmp r8, rm8
                                a[1] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0x3A;    // cmp r8, rm8
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 2: {
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(2);
                                a[0] = 0x3B;    // cmp r16, rm16
                                a[1] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(2 + 1);
                                a[0] = 0x3B;    // cmp r16, rm16
                                a[1] = ToXrm(1, reg_dst, 6);
                                a[2] = (int8_t)op2->location;
                            }
                            break;
                        }
                        case 4: {
                            if (op2->reg != CpuRegister::None) {
                                // Register to register copy
                                uint8_t* a = AllocateBufferForInstruction(3);
                                a[0] = 0x66;    // Operand size prefix
                                a[1] = 0x3B;    // cmp r32, rm32
                                a[2] = ToXrm(3, reg_dst, op2->reg);
                            } else if (op2->location == 0) {
                                ThrowOnUnreachableCode();
                            } else if (op2->location <= INT8_MIN || op2->location >= INT8_MAX) {
                                // Stack to register copy (16-bit range)
                                ThrowOnTooFarCall();
                            } else {
                                // Stack to register copy (8-bit range)
                                uint8_t* a = AllocateBufferForInstruction(3 + 1);
                                a[0] = 0x66;    // Operand size prefix
                                a[1] = 0x3B;    // cmp r32, rm32
                                a[2] = ToXrm(1, reg_dst, 6);
                                a[3] = (int8_t)op2->location;
                            }
                            break;
                        }

                        default: ThrowOnUnreachableCode();
                    }
                    break;
                }

                default: ThrowOnUnreachableCode();
            }

            uint8_t* a = AllocateBufferForInstruction(1 + 1);

            switch (i->if_statement.type) {
                case CompareType::Equal:            a[0] = 0x74; break; // jz rel8
                case CompareType::NotEqual:         a[0] = 0x75; break; // jnz rel8
                case CompareType::Greater:          a[0] = 0x77; break; // jnbe rel8
                case CompareType::Less:             a[0] = 0x72; break; // jb rel8
                case CompareType::GreaterOrEqual:   a[0] = 0x73; break; // jnb rel8
                case CompareType::LessOrEqual:      a[0] = 0x76; break; // jbe rel8

                default: ThrowOnUnreachableCode();
            }

            goto_ptr = a + 1;
            break;
        }

        default: ThrowOnUnreachableCode();
    }

DoBackpatch:
    if (!goto_ptr) {
        ThrowOnUnreachableCode();
    }

    if (i->if_statement.ip < ip_src) {
        uint32_t dst = ip_src_to_dst[i->if_statement.ip];
        *(uint16_t*)goto_ptr = (int16_t)(dst - ip_dst);
        return;
    }

    // Create backpatch info, if the line was not precessed yet
    {
        DosBackpatchInstruction b{};
        b.type = DosBackpatchType::ToRel8;
        b.backpatch_offset = goto_ptr - buffer;
        b.backpatch_ip = ip_dst;
        b.target = DosBackpatchTarget::IP;
        b.ip_src = i->if_statement.ip;
        backpatch.push_back(b);
    }
}

void DosExeEmitter::EmitPush(InstructionEntry* i, std::stack<InstructionEntry*>& call_parameters)
{
	call_parameters.push(i);
}

void DosExeEmitter::EmitCall(InstructionEntry* i, SymbolTableEntry* symbol_table, std::stack<InstructionEntry*>& call_parameters)
{
	// Parameter count mismatch, this should not happen,
	// because "call" instructions are generated by compiler
	if (i->call_statement.target->parameter != call_parameters.size()) {
		ThrowOnUnreachableCode();
	}

	SaveAndUnloadAllRegisters();

	// Emit "push" instructions (evaluated right to left)
	{
		for (int32_t param = i->call_statement.target->parameter; param > 0; param--) {
			InstructionEntry* push = call_parameters.top();
			call_parameters.pop();

			SymbolTableEntry* param_decl = symbol_table;
			while (param_decl) {
				if (param_decl->parameter == param && param_decl->parent && strcmp(param_decl->parent, i->call_statement.target->name) == 0) {
					break;
				}

				param_decl = param_decl->next;
			}

			// Can't find parameter, this should not happen,
			// because function parameters are generated by compiler
			if (!param_decl) {
				ThrowOnUnreachableCode();
			}

			switch (push->push_statement.symbol->expression_type) {
			case ExpressionType::Constant: {
				// Push constant directly to parameter stack
				switch (param_decl->type) {
				case SymbolType::Bool:
				case SymbolType::Uint8: {
					uint8_t imm8 = atoi(push->push_statement.symbol->name);

					uint8_t* a = AllocateBufferForInstruction(1 + 1);
					a[0] = 0x6A;   // push imm8
					a[1] = imm8;
					break;
				}
				case SymbolType::Uint16: {
					uint16_t imm16 = atoi(push->push_statement.symbol->name);

					uint8_t* a = AllocateBufferForInstruction(1 + 2);
					a[0] = 0x68;   // push imm16
					*(uint16_t*)(a + 1) = imm16;
					break;
				}
				case SymbolType::Uint32: {
					uint32_t imm32 = atoi(push->push_statement.symbol->name);

					uint8_t* a = AllocateBufferForInstruction(2 + 4);
					a[0] = 0x66;   // Operand size prefix
					a[1] = 0x68;   // push imm32
					*(uint32_t*)(a + 2) = imm32;
					break;
				}

				case SymbolType::String: {
					strings.insert(push->push_statement.symbol->name);

					uint8_t* a = AllocateBufferForInstruction(1 + 2);
					a[0] = 0x68;   // push imm16

								   // Create backpatch info for string
					{
						DosBackpatchInstruction b{};
						b.target = DosBackpatchTarget::String;
						b.type = DosBackpatchType::ToDsAbs16;
						b.backpatch_offset = (a + 1) - buffer;
						b.value = push->push_statement.symbol->name;
						backpatch.push_back(b);
					}
					break;
				}

										 //case SymbolType::Array: break;

				default: ThrowOnUnreachableCode();
				}

				break;
			}

			case ExpressionType::Variable: {
				DosVariableDescriptor* var_src = FindVariableByName(push->push_statement.symbol->name);
				PushVariableToStack(var_src, param_decl->type);
				break;
			}

			default: ThrowOnUnreachableCode();
			}
		}
	}

	// Emit "call" instruction
	{
		uint8_t* call = AllocateBufferForInstruction(1 + 2);
		call[0] = 0xE8; // call rel16

		std::list<DosLabel>::iterator it = functions.begin();

		while (it != functions.end()) {
			if (strcmp(it->name, i->call_statement.target->name) == 0) {
				*(uint16_t*)(call + 1) = (int16_t)(it->ip_dst - ip_dst);
				goto AlreadyPatched;
			}

			++it;
		}

		// Create backpatch info, if the function was not defined yet
		{
			DosBackpatchInstruction b{};
			b.type = DosBackpatchType::ToRel16;
			b.backpatch_offset = (call + 1) - buffer;
			b.backpatch_ip = ip_dst;
			b.target = DosBackpatchTarget::Function;
			b.value = i->call_statement.target->name;
			backpatch.push_back(b);
		}

	AlreadyPatched:
		;
	}

	if (i->call_statement.target->return_type != ReturnSymbolType::Void) {
		// Set register of return variable to AX
		DosVariableDescriptor* var_dst = FindVariableByName(i->call_statement.return_symbol);
		var_dst->reg = CpuRegister::AX;
	}
}

void DosExeEmitter::EmitReturn(InstructionEntry* i, SymbolTableEntry* symbol_table)
{
	if (parent->type == SymbolType::EntryPoint) {
		switch (i->return_statement.type) {
		case ExpressionType::Constant: {
			uint8_t imm8 = atoi(i->return_statement.value);

			uint8_t* a = AllocateBufferForInstruction(6);
			a[0] = 0xB0;    // mov al, imm8
			a[1] = imm8;    //  Return code
			a[2] = 0xB4;    // mov ah, imm8
			a[3] = 0x4C;    //  Terminate Process With Return Code
			a[4] = 0xCD;    // int imm8
			a[5] = 0x21;    //  DOS Function Dispatcher
			break;
		}
		case ExpressionType::Variable: {
			DosVariableDescriptor* var_src = FindVariableByName(i->return_statement.value);

			if (var_src->reg == CpuRegister::AX) {
				// Value is already in place
			}
			else if (var_src->reg != CpuRegister::None) {
				// Register to register copy
				uint8_t* a = AllocateBufferForInstruction(2);
				a[0] = 0x8A;    // mov r8, rm8
				a[1] = ToXrm(3, CpuRegister::AL, var_src->reg);
			}
			else if (var_src->location <= INT8_MIN || var_src->location >= INT8_MAX) {
				// Stack to register copy (16-bit range)
				ThrowOnUnreachableCode();
			}
			else {
				// Stack to register copy (8-bit range)
				uint8_t* a = AllocateBufferForInstruction(2 + 1);
				a[0] = 0x8A;    // mov r8, rm8
				a[1] = ToXrm(1, CpuRegister::AL, 6);
				a[2] = (int8_t)var_src->location;
			}

			uint8_t* a = AllocateBufferForInstruction(4);
			a[0] = 0xB4;    // mov ah, imm8
			a[1] = 0x4C;    //  Terminate Process With Return Code
			a[2] = 0xCD;    // int imm8
			a[3] = 0x21;    //  DOS Function Dispatcher
			break;
		}

		default: ThrowOnUnreachableCode();
		}
	}
	else {
		// Return value in AX register
		if (parent->return_type != ReturnSymbolType::Void) {
			switch (i->return_statement.type) {
			case ExpressionType::Constant: {
				int32_t value = atoi(i->return_statement.value);

				uint8_t* a = AllocateBufferForInstruction(2 + 4);
				a[0] = 0x66;    // Operand size prefix
				a[1] = 0xB8;    // mov r32 (ax), imm32
				*(uint32_t*)(a + 2) = value;
				break;
			}
			case ExpressionType::Variable: {
				DosVariableDescriptor* var_src = FindVariableByName(i->return_statement.value);
				if (var_src->reg == CpuRegister::AX) {
					// Value is already in place
					break;
				}

				int32_t var_size = compiler->GetSymbolTypeSize(var_src->symbol->type);
				switch (var_size) {
				case 1: {
					if (var_src->reg != CpuRegister::None) {
						// Register to register copy
						uint8_t* a = AllocateBufferForInstruction(2);
						a[0] = 0x8A;    // mov r8, rm8
						a[1] = ToXrm(3, CpuRegister::AX, var_src->reg);
					}
					else if (var_src->location <= INT8_MIN || var_src->location >= INT8_MAX) {
						// Stack to register copy (16-bit range)
						ThrowOnUnreachableCode();
					}
					else {
						// Stack to register copy (8-bit range)
						uint8_t* a = AllocateBufferForInstruction(2 + 1);
						a[0] = 0x8A;    // mov r8, rm8
						a[1] = ToXrm(1, CpuRegister::AX, 6);
						a[2] = (int8_t)var_src->location;
					}
					break;
				}
				case 2: {
					if (var_src->reg != CpuRegister::None) {
						// Register to register copy
						uint8_t* a = AllocateBufferForInstruction(2);
						a[0] = 0x8B;    // mov r16, rm16
						a[1] = ToXrm(3, CpuRegister::AX, var_src->reg);
					}
					else if (var_src->location <= INT8_MIN || var_src->location >= INT8_MAX) {
						// Stack to register copy (16-bit range)
						ThrowOnUnreachableCode();
					}
					else {
						// Stack to register copy (8-bit range)
						uint8_t* a = AllocateBufferForInstruction(2 + 1);
						a[0] = 0x8B;    // mov r16, rm16
						a[1] = ToXrm(1, CpuRegister::AX, 6);
						a[2] = (int8_t)var_src->location;
					}

					break;
				}
				case 4: {
					if (var_src->reg != CpuRegister::None) {
						// Register to register copy
						uint8_t* a = AllocateBufferForInstruction(3);
						a[0] = 0x66;    // Operand size prefix
						a[1] = 0x8B;    // mov r32, rm32
						a[2] = ToXrm(3, CpuRegister::AX, var_src->reg);
					}
					else if (var_src->location <= INT8_MIN || var_src->location >= INT8_MAX) {
						// Stack to register copy (16-bit range)
						ThrowOnUnreachableCode();
					}
					else {
						// Stack to register copy (8-bit range)
						uint8_t* a = AllocateBufferForInstruction(3 + 1);
						a[0] = 0x66;    // Operand size prefix
						a[1] = 0x8B;    // mov r32, rm32
						a[2] = ToXrm(1, CpuRegister::AX, 6);
						a[3] = (int8_t)var_src->location;
					}

					break;
				}

				default: ThrowOnUnreachableCode();
				}
				break;
			}

			default: ThrowOnUnreachableCode();
			}
		}

		// Destroy current call frame
		uint8_t* a = AllocateBufferForInstruction(3 + 2);
		a[0] = 0x66;                            // Operand size prefix
		a[1] = 0x8B;                            // mov r32 (esp), rm32 (ebp)
		a[2] = ToXrm(3, CpuRegister::SP, CpuRegister::BP);
		a[3] = 0x66;                            // Operand size prefix
		a[4] = 0x58 + (uint8_t)CpuRegister::BP; // pop ebp

		if (parent->parameter > 0) {
			// Compute needed space in stack for parameters
			uint16_t stack_size = 0;

			SymbolTableEntry* param_decl = symbol_table;
			while (param_decl) {
				if (param_decl->parameter != 0 && param_decl->parent && strcmp(param_decl->parent, parent->name) == 0) {
					stack_size += compiler->GetSymbolTypeSize(param_decl->type);
				}

				param_decl = param_decl->next;
			}

			uint8_t* a = AllocateBufferForInstruction(3);
			a[0] = 0xC2;   // retn imm16
			*(uint16_t*)(a + 1) = stack_size;
		}
		else {
			uint8_t* a = AllocateBufferForInstruction(1);
			a[0] = 0xC3;   // retn
		}
	}
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
