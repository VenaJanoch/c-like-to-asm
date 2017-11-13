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
            case InstructionType::Goto: break;
            case InstructionType::GotoLabel: break;
            case InstructionType::If: break;
            case InstructionType::Push: break;
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

template<typename T>
T* DosExeEmitter::AllocateBuffer()
{
    return reinterpret_cast<T*>(AllocateBuffer(sizeof(T)));
}


void DosExeEmitter::SaveAndUnloadVariable(DosVariableDescriptor* var)
{
	int32_t var_size = compiler->GetSymbolTypeSize(var->symbol->type);
	switch (var_size) {
	case 1: {
		// Register to stack copy (8-bit range)
		uint8_t* buffer = AllocateBufferForInstruction(2 + 1);
		buffer[0] = 0x88;   // mov rm8, r8
		buffer[1] = ToXrm(1, var->reg, 6);
		buffer[2] = (int8_t)var->location;
		break;
	}
	case 2: {
		// Register to stack copy (8-bit range)
		uint8_t* buffer = AllocateBufferForInstruction(2 + 1);
		buffer[0] = 0x89;   // mov rm16/32, r16/32
		buffer[1] = ToXrm(1, var->reg, 6);
		buffer[2] = (int8_t)var->location;
		break;
	}
	case 4: {
		// Stack to register copy (8-bit range)
		uint8_t* buffer = AllocateBufferForInstruction(3 + 1);
		buffer[0] = 0x66;   // Operand size prefix
		buffer[1] = 0x89;   // mov rm16/32, r16/32
		buffer[2] = ToXrm(1, var->reg, 6);
		buffer[3] = (int8_t)var->location;
		break;
	}

	default: throw CompilerException(CompilerExceptionSource::Unknown, "Internal Error");
	}

	var->reg = CpuRegister::None;
	var->is_dirty = false;
}

void DosExeEmitter::SaveAndUnloadAllRegisters()
{
	// First four 32-registers are generally usable
	std::list<DosVariableDescriptor>::iterator it = variables.begin();

	while (it != variables.end()) {
		if (it->is_dirty && it->reg != CpuRegister::None && it->symbol->parent && strcmp(it->symbol->parent, parent->name) == 0) {
			SaveAndUnloadVariable(&(*it));
		}

		++it;
	}
}