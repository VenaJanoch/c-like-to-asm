#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>

#include "CompilerException.h"
#include "InstructionEntry.h"
#include "SymbolTableEntry.h"

#define Log(text) { std::cout << text << "\r\n"; }


/// <summary>
/// Name of the function that represents application entry point
/// </summary>
#define EntryPointName "main"

// Defines to shorten the code
#define CheckTypeIsValid(type, loc)                                             \
    if (type != SymbolType::Uint8  &&                                           \
        type != SymbolType::Uint16 &&                                           \
        type != SymbolType::Uint32 &&                                           \
        type != SymbolType::Bool) {                                             \
        throw CompilerException(CompilerExceptionSource::Statement,             \
            "Specified type is not allowed", loc.first_line, loc.first_column); \
    }  

#define CheckIsInt(exp, message, loc)                                       \
    if (exp.type != SymbolType::Uint8  &&                                   \
        exp.type != SymbolType::Uint16 &&                                   \
        exp.type != SymbolType::Uint32) {                                   \
        throw CompilerException(CompilerExceptionSource::Statement,         \
            message, loc.first_line, loc.first_column);                     \
    }   

#define CheckIsBool(exp, message, loc)                                      \
    if (exp.type != SymbolType::Bool) {                                     \
        throw CompilerException(CompilerExceptionSource::Statement,         \
            message, loc.first_line, loc.first_column);                     \
    } 

#define CheckIsIntOrBool(exp, message, loc)                                 \
    if (exp.type != SymbolType::Uint8  &&                                   \
        exp.type != SymbolType::Uint16 &&                                   \
        exp.type != SymbolType::Uint32 &&                                   \
        exp.type != SymbolType::Bool) {                                     \
        throw CompilerException(CompilerExceptionSource::Statement,         \
            message, loc.first_line, loc.first_column);                     \
    }

class Compiler
{
public:
    Compiler();
    ~Compiler();

    int OnRun(int argc, wchar_t *argv[]);

    InstructionEntry* AddToStream(InstructionType type, char* code);
    BackpatchList* AddToStreamWithBackpatch(InstructionType type, char* code);
    void BackpatchStream(BackpatchList* list, int32_t new_ip);

	SymbolTableEntry* ToDeclarationList(SymbolType type, const char* name, ExpressionType expression_type);
	void ToParameterList(SymbolType type, const char* name);
	SymbolTableEntry* ToCallParameterList(SymbolTableEntry* queue, SymbolType type, const char* name, ExpressionType expression_type);

    void AddLabel(const char* name, int32_t ip);
    void AddStaticVariable(SymbolType type, const char* name);

	SymbolTableEntry* GetParameter(const char* name);
	SymbolTableEntry* GetFunction(const char* name);

    /// <summary>
    /// Find symbol by name in table
    /// </summary>
    /// <param name="name">Name of symbol</param>
    /// <returns>Symbol entry</returns>
    SymbolTableEntry* FindSymbolByName(const char* name);

	bool CanImplicitCast(SymbolType to, SymbolType from, ExpressionType type);

	SymbolType GetLargestTypeForArithmetic(SymbolType a, SymbolType b);

    /// <summary>
    /// Get next abstract instruction pointer (index)
    /// </summary>
    /// <returns>Instruction pointer</returns>
    int32_t NextIp();

    SymbolTableEntry* GetUnusedVariable(SymbolType type);

    int32_t GetSymbolTypeSize(SymbolType type);
    int32_t GetReturnSymbolTypeSize(ReturnSymbolType type);

private:
    SymbolTableEntry* AddSymbol(const char* name, SymbolType type, ReturnSymbolType return_type,
        ExpressionType expression_type, int32_t ip, int32_t offset_or_size, int32_t parameter, const char* parent);

	const char* SymbolTypeToString(SymbolType type);
	const char* ReturnSymbolTypeToString(ReturnSymbolType type);
	const char* ExpressionTypeToString(ExpressionType type);


    InstructionEntry* instruction_stream_head = nullptr;
    InstructionEntry* instruction_stream_tail = nullptr;
    SymbolTableEntry* symbol_table = nullptr;
    SymbolTableEntry* declaration_queue = nullptr;

    int32_t current_ip = -1;
    int32_t function_ip = 0;

    uint32_t offset_function = 0;
    uint32_t offset_global = 0;

    uint16_t parameter_count = 0;

    uint32_t var_count_bool = 0;
    uint32_t var_count_uint8 = 0;
    uint32_t var_count_uint16 = 0;
    uint32_t var_count_uint32 = 0;

};

/// <summary>
/// Merge two linked list structures of the same type
/// </summary>
template<typename T>
T* MergeLists(T* a, T* b)
{
    if (a && b) {
        T* head = a;
        while (a->next) {
            a = a->next;
        }
        a->next = b;
        return head;
    }

    if (a && !b) {
        return a;
    }

    if (!a && b) {
        return b;
    }

    return nullptr;
}