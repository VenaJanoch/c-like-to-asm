#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <vector>

#include "CompilerException.h"
#include "InstructionEntry.h"
#include "SymbolTableEntry.h"
#include "ScopeType.h"

#define LogVerbose(text) 
//#define LogVerbose(text) { std::cout << text << "\r\n"; }
#define Log(text) { std::cout << text << "\r\n"; }


/// <summary>
/// Name of the function that represents application entry point
/// </summary>
#define EntryPointName "Main"

// Defines to shorten the code
#define TypeIsValid(type)                                                       \
    (type == SymbolType::Uint8  || type == SymbolType::Uint16 ||                \
     type == SymbolType::Uint32 || type == SymbolType::Bool   ||                \
     type == SymbolType::String)

#define CheckTypeIsValid(type, loc)                                             \
    if (type != SymbolType::Uint8  && type != SymbolType::Uint16 &&             \
        type != SymbolType::Uint32 && type != SymbolType::Bool   &&             \
        type != SymbolType::String) {                                           \
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

#define CheckIsConstant(exp, loc)                                           \
    if (exp.exp_type != ExpressionType::Constant) {                         \
        throw CompilerException(CompilerExceptionSource::Statement,         \
            "Specified expression must have constant value", loc.first_line, loc.first_column); \
    } 

#define FillInstructionForAssign(i, assign_type, dst, op1_, op2_)           \
    {                                                                       \
        i->assignment.type = assign_type;                                   \
        i->assignment.dst_value = dst->name;                                \
        i->assignment.op1.value = op1_.value;                               \
        i->assignment.op1.type = op1_.type;                                 \
        i->assignment.op1.exp_type = op1_.exp_type;                  \
        i->assignment.op2.value = op2_.value;                               \
        i->assignment.op2.type = op2_.type;                                 \
        i->assignment.op2.exp_type = op2_.exp_type;                  \
    }

#define CreateIfWithBackpatch(backpatch, compare_type, op1_, op2_)          \
    {                                                                       \
        backpatch = c.AddToStreamWithBackpatch(InstructionType::If, output_buffer); \
        backpatch->entry->if_statement.type = compare_type;                 \
        backpatch->entry->if_statement.op1.value = op1_.value;              \
        backpatch->entry->if_statement.op1.type = op1_.type;                \
        backpatch->entry->if_statement.op1.exp_type = op1_.exp_type; \
        backpatch->entry->if_statement.op2.value = op2_.value;              \
        backpatch->entry->if_statement.op2.type = op2_.type;                \
        backpatch->entry->if_statement.op2.exp_type = op2_.exp_type; \
    }

#define CreateIfConstWithBackpatch(backpatch, compare_type, op1_, constant)     \
    {                                                                           \
        backpatch = c.AddToStreamWithBackpatch(InstructionType::If, output_buffer); \
        backpatch->entry->if_statement.type = compare_type;                     \
        backpatch->entry->if_statement.op1.value = op1_.value;                  \
        backpatch->entry->if_statement.op1.type = op1_.type;                    \
        backpatch->entry->if_statement.op1.exp_type = op1_.exp_type;     \
        backpatch->entry->if_statement.op2.value = constant;                    \
        backpatch->entry->if_statement.op2.type = op1_.type;                    \
        backpatch->entry->if_statement.op2.exp_type = ExpressionType::Constant; \
    }


class Compiler
{
public:
    Compiler();
    ~Compiler();

    int OnRun(int argc, wchar_t *argv[]);

    void CreateDebugOutput(FILE* output_file);


    void ParseCompilerDirective(char* directive);

    InstructionEntry* AddToStream(InstructionType type, char* code);
    BackpatchList* AddToStreamWithBackpatch(InstructionType type, char* code);
    void BackpatchStream(BackpatchList* list, int32_t new_ip);


    SymbolTableEntry* ToDeclarationList(SymbolType type, const char* name, ExpressionType exp_type);
    void ToParameterList(SymbolType type, const char* name);
    SymbolTableEntry* ToCallParameterList(SymbolTableEntry* queue, SymbolType type, const char* name, ExpressionType exp_type);

    void AddLabel(const char* name, int32_t ip);
    void AddStaticVariable(SymbolType type, const char* name);
    void AddFunction(char* name, ReturnSymbolType return_type);
    void AddFunctionPrototype(char* name, ReturnSymbolType return_type);

    void PrepareForCall(const char* name, SymbolTableEntry* call_parameters, int32_t parameter_count);

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

    void IncreaseScope(ScopeType type);
    void BackpatchScope(ScopeType type, int32_t new_ip);
    bool AddToScopeList(ScopeType type, BackpatchList* backpatch);

private:
    SymbolTableEntry* AddSymbol(const char* name, SymbolType type, ReturnSymbolType return_type,
        ExpressionType exp_type, int32_t ip, int32_t offset_or_size, int32_t parameter, const char* parent, bool is_temp);

    const char* SymbolTypeToString(SymbolType type);
    const char* ReturnSymbolTypeToString(ReturnSymbolType type);
    const char* ExpressionTypeToString(ExpressionType type);

    void ReleaseDeclarationQueue();
    void ReleaseAll();

    void SortSymbolTable();

    /// <summary>
    /// Declare all shared functions, so they can be eventually called
    /// </summary>
    void DeclareSharedFunctions();

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
    uint32_t var_count_string = 0;

    std::vector<BackpatchList*> break_list;
    std::vector<BackpatchList*> continue_list;
    int32_t break_scope = -1;
    int32_t continue_scope = -1;

    uint32_t stack_size = 0;
    
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