#pragma once

#include <stdint.h>

#include "SymbolTableEntry.h"

enum struct InstructionType {
    Unknown,

    Assign,
    Goto,
    GotoLabel,
    If,
    Push,
    Call,
    Return,
};

enum struct AssignType {
    None,
    Negation,

    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
    ShiftLeft,
    ShiftRight,
};

enum struct CompareType {
    None,

    LogOr,
    LogAnd,

    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterOrEqual,
    LessOrEqual
};

struct InstructionEntry {
    char* content;
    int32_t goto_ip;

    InstructionType type;

    union {
        struct {
            AssignType type;

            char* dst_value;

            char* op1_value;
            SymbolType op1_type;
            ExpressionType op1_exp_type;

            char* op2_value;
            SymbolType op2_type;
            ExpressionType op2_exp_type;
        } assignment;

        struct {
            uint32_t goto_ip;
        } goto_statement;

        struct {
            char* label;
        } goto_label_statement;

        struct {
            int32_t goto_ip;

            CompareType type;

            char* op1_value;
            SymbolType op1_type;
            ExpressionType op1_exp_type;

            char* op2_value;
            SymbolType op2_type;
            ExpressionType op2_exp_type;
        } if_statement;

        struct {
            SymbolTableEntry* symbol;
        } push_statement;

        struct {
            SymbolTableEntry* target;
            char* return_symbol;
        } call_statement;

        struct {
            char* value;
            ExpressionType type;
        } return_statement;
    };

    InstructionEntry* next;
};

struct BackpatchList {
    InstructionEntry* entry;

    BackpatchList* next;
};