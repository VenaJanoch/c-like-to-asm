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

struct InstructionOperand {
    char* value;
    SymbolType type;
    ExpressionType exp_type;
};

struct InstructionEntry {
    char* content;
    int32_t goto_ip;

    InstructionType type;

    union {
        struct {
            AssignType type;

            char* dst_value;

            InstructionOperand op1;
            InstructionOperand op2;
        } assignment;

        struct {
            uint32_t ip;
        } goto_statement;

        struct {
            char* label;
        } goto_label_statement;

        struct {
            uint32_t ip;

            CompareType type;

            InstructionOperand op1;
            InstructionOperand op2;
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