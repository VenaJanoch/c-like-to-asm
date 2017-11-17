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


enum struct DosBackpatchType {
    Unknown,

    ToRel8,     // Relative address (8-bit)
    ToRel16,    // Relative address (16-bit)
    ToDsAbs16,  // Absolute address with DS segment (16-bit)
};

enum struct DosBackpatchTarget {
    Unknown,

    IP,
    Label,
    Function,
    String
};

struct DosBackpatchInstruction {
    DosBackpatchType type;
    DosBackpatchTarget target;

    uint32_t backpatch_offset;
    uint32_t backpatch_ip;

    int32_t ip_src;
    char* value;
};

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

struct DosVariableDescriptor {
    SymbolTableEntry* symbol;

    char* value;

    bool is_static;
    CpuRegister reg;
    int32_t location;

    uint32_t last_used;

    bool is_dirty;
};

struct DosLabel {
    char* name;
    int32_t ip_dst;
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

#pragma pack(pop)

// Defines to shorten the code
#define ToOpR(op, r)  \
    (uint8_t)(op + ((uint8_t)(r) & 0x07))

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

	/// <summary>
	/// Return unused/free register, if all registers are referenced,
	/// save and unreference least used register
	/// </summary>
	/// <returns>Unused register</returns>
	CpuRegister GetUnusedRegister();

	/// <summary>
	/// Find variable specified by name in variable list
	/// </summary>
	/// <param name="name">Name of variable</param>
	/// <returns>Variable descriptor</returns>
	DosVariableDescriptor* FindVariableByName(char* name);

    /// <summary>
    /// Save specified variable to stack, but keep it in register
    /// </summary>
    /// <param name="var"></param>
    void SaveVariable(DosVariableDescriptor* var);

    /// <summary>
    /// Save variable which uses specified register and unreference it
    /// </summary>
    /// <param name="reg">Register to unreference</param>
    void SaveAndUnloadRegister(CpuRegister reg);

    /// <summary>
    /// Save all unsaved variables to stack and unreference all registers
    /// </summary>
    void SaveAndUnloadAllRegisters();

    /// <summary>
    /// Destroy connection of variable with register
    /// If the variable is unsaved, compiler exception is thrown
    /// </summary>
    /// <param name="reg">Discarded register</param>
    void MarkRegisterAsDiscarded(CpuRegister reg);

    /// <summary>
    /// Push value of variable to parameter stack 
    /// </summary>
    /// <param name="var">Variable to push</param>
    /// <param name="param_type">Type of parameter</param>
    void PushVariableToStack(DosVariableDescriptor* var, SymbolType param_type);

    /// <summary>
    /// Force load value of variable to any register,
    /// if it is already in register, ownership will be removed
    /// </summary>
    /// <param name="var">Variable to copy</param>
    /// <param name="desired_size">Target size</param>
    /// <returns>Target register</returns>
    CpuRegister LoadVariableUnreferenced(DosVariableDescriptor* var, int32_t desired_size);

    /// <summary>
    /// Force copy value of variable to specified register,
    /// it does not change ownership of any register
    /// </summary>
    /// <param name="var">Variable to copy</param>
    /// <param name="reg_dst">Target register</param>
    /// <param name="desired_size">Target size</param>
    void CopyVariableToRegister(DosVariableDescriptor* var, CpuRegister reg_dst, int32_t desired_size);

    /// <summary>
    /// Load constant value to specified register with smallest possible size
    /// </summary>
    /// <param name="value">Constant value</param>
    /// <param name="reg">Target register</param>
    void LoadConstantToRegister(int32_t value, CpuRegister reg);

    /// <summary>
    /// Load constant value to specified register with specified size
    /// </summary>
    /// <param name="value">Constant value</param>
    /// <param name="reg">Target register</param>
    /// <param name="desired_size">Specified size</param>
    void LoadConstantToRegister(int32_t value, CpuRegister reg, int32_t desired_size);

    /// <summary>
    /// Fill specified register with zeros
    /// </summary>
    /// <param name="reg">Register</param>
    /// <param name="desired_size">Size of register</param>
    void ZeroRegister(CpuRegister reg, int32_t desired_size);

	// Backpatching
	void BackpatchAddresses();
	void BackpatchLabels(DosLabel& label, DosBackpatchTarget target);
	void CheckBackpatchListIsEmpty(DosBackpatchTarget target);

	// Instruction emitters
    void EmitEntryPointPrologue(SymbolTableEntry* function);

	void EmitFunctionPrologue(SymbolTableEntry* function, SymbolTableEntry* symbol_table);

	void EmitAssign(InstructionEntry* i);
	void EmitGoto(InstructionEntry* i);
	void EmitGotoLabel(InstructionEntry* i);
	void EmitIf(InstructionEntry* i);
	void EmitPush(InstructionEntry* i, std::stack<InstructionEntry*>& call_parameters);
	void EmitCall(InstructionEntry* i, SymbolTableEntry* symbol_table, std::stack<InstructionEntry*>& call_parameters);
	void EmitReturn(InstructionEntry* i, SymbolTableEntry* symbol_table);


	/// <summary>
	/// Get opposite compare type, so operands can be swapped
	/// </summary>
	/// <param name="type">Compare type</param>
	/// <returns>Opposite compare type</returns>
	CompareType GetSwappedCompareType(CompareType type);

	/// <summary>
	/// Do compare of two constant values at compile-time
	/// </summary>
	/// <param name="type">Compare type</param>
	/// <param name="op1">Operand 1</param>
	/// <param name="op2">Operand 2</param>
	/// <returns></returns>
	bool IfConstexpr(CompareType type, int32_t op1, int32_t op2);

    // Output buffer management
    uint8_t* AllocateBuffer(uint32_t size);
    uint8_t* AllocateBufferForInstruction(uint32_t size);

    template<typename T>
    T* AllocateBuffer();


    Compiler* compiler;

    uint32_t ip_src = 0;
    uint32_t ip_dst = 0;

	std::map<uint32_t, uint32_t> ip_src_to_dst;
	std::list<DosBackpatchInstruction> backpatch;
	std::list<DosVariableDescriptor> variables;
	std::list<DosLabel> functions;
	std::list<DosLabel> labels;
	std::unordered_set<char*> strings;

    SymbolTableEntry* parent = nullptr;

    uint8_t* buffer = nullptr;
    uint32_t buffer_offset = 0;
    uint32_t buffer_size = 0;
};