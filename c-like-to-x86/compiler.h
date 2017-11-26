#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <functional>

#include "CompilerException.h"
#include "InstructionEntry.h"
#include "SymbolTableEntry.h"
#include "ScopeType.h"


/// <summary>
/// Name of the function that represents application entry point
/// </summary>
#define EntryPointName "Main"

#define IsPointer (-1)

// Defines to shorten the code
#define TypeIsValid(type)                                                       \
    (type == SymbolType::Uint8  || type == SymbolType::Uint16 ||                \
     type == SymbolType::Uint32 || type == SymbolType::Bool   ||                \
     type == SymbolType::String)

#define CheckTypeIsValid(type, loc)                                             \
    if (!TypeIsValid(type)) {                                                   \
        throw CompilerException(CompilerExceptionSource::Statement,             \
            "Specified type is not allowed", loc.first_line, loc.first_column); \
    }  

#define CheckTypeIsArrayCompatible(type, message, loc)                      \
    if (type != SymbolType::Uint8  &&                                       \
        type != SymbolType::Uint16 &&                                       \
        type != SymbolType::Uint32 &&                                       \
        type != SymbolType::Bool) {                                         \
        throw CompilerException(CompilerExceptionSource::Statement,         \
            message, loc.first_line, loc.first_column);                     \
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
            "Specified expression must have constant value",                \
            loc.first_line, loc.first_column);                              \
    } 

#define CopyOperand(to, from)                       \
    {                                               \
        to.value = from.value;                      \
        to.type = from.type;                        \
        to.exp_type = from.exp_type;                \
        to.index.value = from.index.value;          \
        to.index.type = from.index.type;            \
        to.index.exp_type = from.index.exp_type;    \
    }

#define FillInstructionForAssign(i, assign_type, dst, op1_, op2_)           \
    {                                                                       \
        i->assignment.type = assign_type;                                   \
        i->assignment.dst_value = dst->name;                                \
        CopyOperand(i->assignment.op1, op1_);                               \
        CopyOperand(i->assignment.op2, op2_);                               \
    }

#define CreateIfWithBackpatch(backpatch, compare_type, op1_, op2_)              \
    {                                                                           \
        backpatch = c.AddToStreamWithBackpatch(InstructionType::If, output_buffer); \
        backpatch->entry->if_statement.type = compare_type;                     \
        CopyOperand(backpatch->entry->if_statement.op1, op1_);                  \
        CopyOperand(backpatch->entry->if_statement.op2, op2_);                  \
    }

#define CreateIfConstWithBackpatch(backpatch, compare_type, op1_, constant)     \
    {                                                                           \
        backpatch = c.AddToStreamWithBackpatch(InstructionType::If, output_buffer); \
        backpatch->entry->if_statement.type = compare_type;                     \
        CopyOperand(backpatch->entry->if_statement.op1, op1_);                  \
        backpatch->entry->if_statement.op2.value = constant;                    \
        backpatch->entry->if_statement.op2.type = op1_.type;                    \
        backpatch->entry->if_statement.op2.exp_type = ExpressionType::Constant; \
    }

#define PrepareIndexedVariableIfNeeded(var)                                     \
    if (var.exp_type == ExpressionType::Variable && var.index.value) {          \
        SymbolTableEntry* _decl_index = c.GetUnusedVariable(var.type);          \
                                                                                \
        sprintf_s(output_buffer, "%s = %s[%s]", _decl_index->name, var.value, var.index.value); \
        InstructionEntry* _i = c.AddToStream(InstructionType::Assign, output_buffer);           \
        _i->assignment.dst_value = _decl_index->name;                           \
        CopyOperand(_i->assignment.op1, var);                                   \
                                                                                \
        var.value = _decl_index->name;                                          \
        var.type = _decl_index->type;                                           \
        var.exp_type = ExpressionType::Variable;                                \
    }

#define PrepareIndexedVariableIfNeededMarker(var, marker)                       \
    if (var.exp_type == ExpressionType::Variable && var.index.value) {          \
        SymbolTableEntry* _decl_index = c.GetUnusedVariable(var.type);          \
                                                                                \
        sprintf_s(output_buffer, "%s = %s[%s]", _decl_index->name, var.value, var.index.value); \
        InstructionEntry* _i = c.AddToStream(InstructionType::Assign, output_buffer);           \
        _i->assignment.dst_value = _decl_index->name;                           \
        CopyOperand(_i->assignment.op1, var);                                   \
                                                                                \
        var.value = _decl_index->name;                                          \
        var.type = _decl_index->type;                                           \
        var.exp_type = ExpressionType::Variable;                                \
                                                                                \
        marker.ip += 1;                                                         \
    }

#define PrepareExpressionsForLogical(exp1, marker, exp2)                        \
    {                                                                           \
        if (exp1.type != SymbolType::Bool) {                                    \
                sprintf_s(output_buffer, "if (%s != 0) goto", exp1.value);      \
                CreateIfConstWithBackpatch(exp1.true_list, CompareType::NotEqual, exp1, "0");       \
                sprintf_s(output_buffer, "goto");                               \
                exp1.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer); \
                                                                                \
                marker.ip += 2;                                                 \
        }                                                                       \
        if (exp2.type != SymbolType::Bool) {                                    \
                sprintf_s(output_buffer, "if (%s != 0) goto", exp2.value);      \
                CreateIfConstWithBackpatch(exp2.true_list, CompareType::NotEqual, exp2, "0");       \
                sprintf_s(output_buffer, "goto");                               \
                exp2.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer); \
        }                                                                       \
    }

#define PreAssign(exp)                                                          \
    int32_t _true_ip, _false_ip;                                                \
    {                                                                           \
        _true_ip = c.NextIp();                                                  \
        if (exp.true_list || exp.false_list) {                                  \
            sprintf_s(output_buffer, "%s = 1", exp.value);                      \
            InstructionEntry* _i = c.AddToStream(InstructionType::Assign, output_buffer);   \
            _i->assignment.type = AssignType::None;                             \
            _i->assignment.dst_value = exp.value;                               \
            _i->assignment.op1.value = _strdup("1");                            \
            _i->assignment.op1.type = SymbolType::Bool;                         \
            _i->assignment.op1.exp_type = ExpressionType::Constant;             \
        }                                                                       \
        _false_ip = c.NextIp();                                                 \
    }                                                                           \

#define PostAssign(res, exp)                                                    \
    {                                                                           \
        if (exp.true_list || exp.false_list) {                                  \
            if (!exp.true_list || !exp.false_list) {                            \
                ThrowOnUnreachableCode();                                       \
            }                                                                   \
            c.BackpatchStream(exp.true_list, _true_ip);                         \
            c.BackpatchStream(exp.false_list, _false_ip);                       \
        }                                                                       \
        c.ResetScope(ScopeType::Assign);                                        \
        res.true_list = nullptr;                                                \
        res.false_list = nullptr;                                               \
    }

#define PreIf()                                                                 \
    SymbolTableEntry* _decl_if = nullptr;                                       \
    {                                                                           \
        if (c.IsScopeActive(ScopeType::Assign)) {                               \
            _decl_if = c.GetUnusedVariable(SymbolType::Bool);                   \
                                                                                \
            sprintf_s(output_buffer, "%s = 0", _decl_if->name);                 \
            InstructionEntry* _i = c.AddToStream(InstructionType::Assign, output_buffer);   \
            _i->assignment.type = AssignType::None;                             \
            _i->assignment.dst_value = _decl_if->name;                          \
            _i->assignment.op1.value = _strdup("0");                            \
            _i->assignment.op1.type = SymbolType::Bool;                         \
            _i->assignment.op1.exp_type = ExpressionType::Constant;             \
        }                                                                       \
    }

#define PreIfMarker(marker)                                                     \
    SymbolTableEntry* _decl_if = nullptr;                                       \
    {                                                                           \
        if (c.IsScopeActive(ScopeType::Assign)) {                               \
            _decl_if = c.GetUnusedVariable(SymbolType::Bool);                   \
                                                                                \
            sprintf_s(output_buffer, "%s = 0", _decl_if->name);                 \
            InstructionEntry* _i = c.AddToStream(InstructionType::Assign, output_buffer);   \
            _i->assignment.type = AssignType::None;                             \
            _i->assignment.dst_value = _decl_if->name;                          \
            _i->assignment.op1.value = _strdup("0");                            \
            _i->assignment.op1.type = SymbolType::Bool;                         \
            _i->assignment.op1.exp_type = ExpressionType::Constant;             \
                                                                                \
            marker.ip += 1;                                                     \
        }                                                                       \
    }

#define PostIf(res, exp)                                                        \
    {                                                                           \
        if (c.IsScopeActive(ScopeType::Assign)) {                               \
            res.value = _decl_if->name;                                         \
            res.exp_type = ExpressionType::Variable;                            \
                                                                                \
            c.ResetScope(ScopeType::Assign);                                    \
        } else {                                                                \
            res.exp_type = exp.exp_type;                                        \
        }                                                                       \
                                                                                \
        res.type = SymbolType::Bool;                                            \
        res.index.value = nullptr;                                              \
    }

class Compiler
{
public:
    Compiler();
    ~Compiler();

    int OnRun(int argc, wchar_t *argv[]);

#if _DEBUG
    void CreateDebugOutput();
#endif

    void ParseCompilerDirective(char* directive, std::function<bool(char* directive, char* param)> callback);

    InstructionEntry* AddToStream(InstructionType type, char* code);
    BackpatchList* AddToStreamWithBackpatch(InstructionType type, char* code);
    void BackpatchStream(BackpatchList* list, int32_t new_ip);

    SymbolTableEntry* GetSymbols();

    SymbolTableEntry* ToDeclarationList(SymbolType type, int32_t size, const char* name, ExpressionType exp_type);
    void ToParameterList(SymbolType type, const char* name);
    SymbolTableEntry* ToCallParameterList(SymbolTableEntry* queue, SymbolType type, const char* name, ExpressionType exp_type);

    void AddLabel(const char* name, int32_t ip);
    void AddStaticVariable(SymbolType type, int32_t size, const char* name);
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

    int32_t GetSymbolTypeSize(SymbolType type, bool is_pointer);
    SymbolType ReturnSymbolTypeToSymbolType(ReturnSymbolType type);

    void IncreaseScope(ScopeType type);
    void ResetScope(ScopeType type);
    bool IsScopeActive(ScopeType type);
    void BackpatchScope(ScopeType type, int32_t new_ip);
    bool AddToScopeList(ScopeType type, BackpatchList* backpatch);

    const char* SymbolTypeToString(SymbolType type);
    const char* ReturnSymbolTypeToString(ReturnSymbolType type);

private:
    SymbolTableEntry* AddSymbol(const char* name, SymbolType type, int32_t size, ReturnSymbolType return_type,
        ExpressionType exp_type, int32_t ip, int32_t offset_or_size, int32_t parameter, const char* parent, bool is_temp);

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
    int32_t assign_scope = 0;
    int32_t break_scope = -1;
    int32_t continue_scope = -1;

    uint32_t stack_size = 0;
    
};

/// <summary>
/// Merge two raw linked list structures of the same type
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