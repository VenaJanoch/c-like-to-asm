#pragma once

#include <stdint.h>

/// <summary>
/// All available types of symbols
/// </summary>
enum struct SymbolType {
    Unknown,
    None,

    Label,

    Bool,
    Uint8,
    Uint16,
    Uint32,
    String,

    Array, // ToDo
};

/// <summary>
/// Types of symbols which can be used as return of the function
/// </summary>
enum struct ReturnSymbolType {
    Unknown,

    Void,
    Bool,
    Uint8,
    Uint16,
    Uint32,
    String,
};

enum struct ExpressionType {
	None,

	Constant,
	Variable,
};

