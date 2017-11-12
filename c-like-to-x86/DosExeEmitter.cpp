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
   
    current = instruction_stream;
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
