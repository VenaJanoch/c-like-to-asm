#include "Compiler.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <string>

// Internal Bison functions and variables
extern int yylex();
extern int yyparse();
extern FILE* yyin;

Compiler::Compiler()
{
}

Compiler::~Compiler()
{
}

int Compiler::OnRun(int argc, wchar_t* argv[])
{
    bool is_file;

    if (argc >= 2) {
        errno_t err = _wfopen_s(&yyin, argv[1], L"rb");
        if (err) {
            wchar_t error[200];
            _wcserror_s(error, err);
            std::wcout << "Error: " << error;
            return EXIT_FAILURE;
        }

        is_file = true;
    } else {
        yyin = stdin;
        is_file = false;
    }

    do { 
    	yyparse();
    } while(!feof(yyin));

    if (is_file) {
        fclose(yyin);
    }

    return EXIT_SUCCESS;
}

InstructionEntry* Compiler::AddToStream(InstructionType type, char* code)
{
    Log(":: " << code);

    InstructionEntry* entry = new InstructionEntry();
    entry->content = _strdup(code);
    entry->goto_ip = -1;
    entry->type = type;

    if (instruction_stream_head) {
        instruction_stream_tail->next = entry;
        instruction_stream_tail = entry;
    } else {
        instruction_stream_head = entry;
        instruction_stream_tail = entry;
    }

    // Advance abstract instruction pointer
    current_ip++;

    return entry;
}

BackpatchList* Compiler::AddToStreamWithBackpatch(InstructionType type, char* code)
{
    InstructionEntry* entry = AddToStream(type, code);

    BackpatchList* backpatch = new BackpatchList();
    backpatch->entry = entry;
    return backpatch;
}

void Compiler::BackpatchStream(BackpatchList* list, int32_t new_ip)
{
    while (list) {
        if (list->entry) {
            // Apply new abstract instruction pointer value
            if (list->entry->type == InstructionType::Goto) {
                list->entry->goto_statement.goto_ip = new_ip;
            } else if (list->entry->type == InstructionType::If) {
                list->entry->if_statement.goto_ip = new_ip;
            } else {
                // This type cannot be backpatched...
                Log("Trying to backpatch unsupported instruction");
            }
        }

        // Release entry in backpatch linked list
        BackpatchList* current = list;
        list = list->next;
        delete current;
    }
}

bool Compiler::CanImplicitCast(SymbolType to, SymbolType from, ExpressionType type)
{
	if (from == to) {
		return true;
	}

	if (type == ExpressionType::Constant) {
		if ((from == SymbolType::Uint8 ||
			from == SymbolType::Uint16 ||
			from == SymbolType::Uint32) &&
			(to == SymbolType::Uint8 ||
				to == SymbolType::Uint16 ||
				to == SymbolType::Uint32)) {

			// Constant implicit cast
			return true;
		}
	}

	if (to >= SymbolType::Bool && from >= SymbolType::Bool &&
		to <= SymbolType::Uint32 && from <= SymbolType::Uint32) {

		if (to >= from) {
			// Only expansion (assign smaller to bigger type) is detected as implicit cast
			return true;
		}
	}

	return false;
}

SymbolType Compiler::GetLargestTypeForArithmetic(SymbolType a, SymbolType b)
{
	if (a == SymbolType::Uint32 || b == SymbolType::Uint32)
		return SymbolType::Uint32;
	if (a == SymbolType::Uint16 || b == SymbolType::Uint16)
		return SymbolType::Uint16;
	if (a == SymbolType::Uint8 || b == SymbolType::Uint8)
		return SymbolType::Uint8;

	return SymbolType::Unknown;
}


const char* Compiler::SymbolTypeToString(SymbolType type)
{
	switch (type) {
	
	case SymbolType::Label: return "Label";

	case SymbolType::Bool: return "bool";
	case SymbolType::Uint8: return "uint8";
	case SymbolType::Uint16: return "uint16";
	case SymbolType::Uint32: return "uint32";
	case SymbolType::String: return "string";

	case SymbolType::Array: return "Array";

	default: return "-";
	}
}

const char* Compiler::ReturnSymbolTypeToString(ReturnSymbolType type)
{
	switch (type) {
	case ReturnSymbolType::Void: return "void";
	case ReturnSymbolType::Bool: return "bool";
	case ReturnSymbolType::Uint8: return "uint8";
	case ReturnSymbolType::Uint16: return "uint16";
	case ReturnSymbolType::Uint32: return "uint32";
	case ReturnSymbolType::String: return "string";

	default: return "-";
	}
}

const char* Compiler::ExpressionTypeToString(ExpressionType type)
{
	switch (type) {
	case ExpressionType::Constant: return "Const.";
	case ExpressionType::Variable: return "Var.";

	default: return "-";
	}
}

int32_t Compiler::GetSymbolTypeSize(SymbolType type)
{
	switch (type) {
	case SymbolType::Bool: return 1;
	case SymbolType::Uint8: return 1;
	case SymbolType::Uint16: return 2;
	case SymbolType::Uint32: return 4;

	default: throw CompilerException(CompilerExceptionSource::Unknown, "Internal error");
	}
}
