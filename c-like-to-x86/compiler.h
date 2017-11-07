#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>

#include "CompilerException.h"
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

	bool CanImplicitCast(SymbolType to, SymbolType from, ExpressionType type);

	SymbolType GetLargestTypeForArithmetic(SymbolType a, SymbolType b);


private:
	const char* SymbolTypeToString(SymbolType type);
	const char* ReturnSymbolTypeToString(ReturnSymbolType type);
	const char* ExpressionTypeToString(ExpressionType type);

	int32_t GetSymbolTypeSize(SymbolType type);

};
