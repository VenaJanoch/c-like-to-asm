#pragma once

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <string>
#include <list>
#include <map>
#include <stack>
#include <unordered_set>

#include "Compiler.h"
#include "InstructionEntry.h"
#include "SymbolTableEntry.h"

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

#pragma pack(push, 1)

struct MzHeader {
    uint8_t signature[2]; // MZ
    uint16_t last_block_size;
    uint16_t block_count;
    uint16_t reloc_count;
    uint16_t header_paragraphs;
    uint16_t min_extra_paragraphs;
    uint16_t max_extra_paragraphs;
    uint16_t ss;
    uint16_t sp;
    uint16_t checksum;
    uint16_t ip;
    uint16_t cs;
    uint16_t reloc_table_offset;
    uint16_t overlay_count;
};


struct MzRelocEntry {
    uint16_t offset;
    uint16_t segment;
};


struct DosVariableDescriptor {
	SymbolTableEntry* symbol;

	char* value;

	bool is_static;
	CpuRegister reg;
	int32_t location;

	uint32_t last_used;

	bool is_dirty;
};
#pragma pack(pop)

// Defines to shorten the code
#define ToXrm(x, r, m)  \
    (uint8_t)((((uint8_t)(x) << 6) & 0xC0) | (((uint8_t)(r) << 3) & 0x38) | ((uint8_t)(m) & 0x07))


class DosExeEmitter
{
public:
    DosExeEmitter(Compiler* compiler);
    ~DosExeEmitter();

    void EmitMzHeader();
    void EmitInstructions(InstructionEntry* instruction_stream, SymbolTableEntry* symbol_table);
    void EmitSharedFunctions(SymbolTableEntry* symbol_table);
    void EmitStaticData();

    void Save(FILE* stream);

private:

	void CreateVariableList(SymbolTableEntry* symbol_table);

    // Output buffer management
    uint8_t* AllocateBuffer(uint32_t size);

	DosVariableDescriptor* FindVariableByName(char* name);

	void SaveAndUnloadVariable(DosVariableDescriptor* var);

	void SaveAndUnloadAllRegisters();


	// Buffer management
	uint8_t* AllocateBuffer(uint32_t size);
	uint8_t* AllocateBufferForInstruction(uint32_t size);

    template<typename T>
    T* AllocateBuffer();


	std::list<DosVariableDescriptor> variables;

    Compiler* compiler;

	SymbolTableEntry* parent = nullptr;

    uint8_t* buffer = nullptr;
    uint32_t buffer_offset = 0;
    uint32_t buffer_size = 0;
};