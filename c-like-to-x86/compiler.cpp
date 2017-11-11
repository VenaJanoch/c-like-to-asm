#include "Compiler.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <string>

// Internal Bison functions and variables
extern int yylex();
extern int yyparse();
extern FILE* yyin;
extern int yylineno;

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

SymbolTableEntry* Compiler::FindSymbolByName(const char* name)
{
    SymbolTableEntry* current = symbol_table;
    while (current) {
        if (current->name && strcmp(name, current->name) == 0) {
            break;
        }

        current = current->next;
    }

    return current;
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

int32_t Compiler::NextIp()
{
    return current_ip + 1;
}

SymbolTableEntry* Compiler::GetUnusedVariable(SymbolType type)
{
    char buffer[20];

    switch (type) {
        case SymbolType::Bool: {
            var_count_bool++;
            sprintf_s(buffer, "__b_%d", var_count_bool);
            break;
        }

        case SymbolType::Uint8: {
            var_count_uint8++;
            sprintf_s(buffer, "__ui8_%d", var_count_uint8);
            break;
        }

        case SymbolType::Uint16: {
            var_count_uint16++;
            sprintf_s(buffer, "__ui16_%d", var_count_uint16);
            break;
        }

        case SymbolType::Uint32: {
            var_count_uint32++;
            sprintf_s(buffer, "__ui32_%d", var_count_uint32);
            break;
        }

        default: throw CompilerException(CompilerExceptionSource::Unknown, "Internal error");
    }

    return ToDeclarationList(type, buffer, ExpressionType::Variable);
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

SymbolTableEntry* Compiler::ToDeclarationList(SymbolType type, const char* name, ExpressionType expression_type)
{
	SymbolTableEntry* symbol = new SymbolTableEntry();
	symbol->name = _strdup(name);
	symbol->type = type;
	symbol->expression_type = expression_type;
	symbol->offset_or_size = GetSymbolTypeSize(type);

	if (declaration_queue) {
		SymbolTableEntry* entry = declaration_queue;
		while (entry->next) {
			if (strcmp(name, entry->name) == 0) {
				std::string message = "Variable \"";
				message += name;
				message += "\" is already declared in this scope";
				throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
			}
			entry = entry->next;
		}
		entry->next = symbol;
	}
	else {
		declaration_queue = symbol;
	}

	return symbol;
}

void Compiler::ToParameterList(SymbolType type, const char* name)
{
	parameter_count++;

	SymbolTableEntry* symbol = new SymbolTableEntry();
	symbol->name = _strdup(name);
	symbol->type = type;
	symbol->offset_or_size = GetSymbolTypeSize(type);
	symbol->parameter = parameter_count;

	if (declaration_queue) {
		SymbolTableEntry* entry = declaration_queue;
		while (entry->next) {
			if (strcmp(name, entry->name) == 0) {
				std::string message = "Parameter \"";
				message += name;
				message += "\" is already declared in this scope";
				throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
			}
			entry = entry->next;
		}
		entry->next = symbol;
	}
	else {
		declaration_queue = symbol;
	}
}

SymbolTableEntry* Compiler::ToCallParameterList(SymbolTableEntry* list, SymbolType type, const char* name, ExpressionType expression_type)
{
	SymbolTableEntry* symbol = new SymbolTableEntry();
	symbol->name = _strdup(name);
	symbol->type = type;
	symbol->expression_type = expression_type;

	if (list) {
		SymbolTableEntry* entry = list;
		while (entry->next) {
			entry = entry->next;
		}
		entry->next = symbol;
		return list;
	}
	else {
		return symbol;
	}
}

void Compiler::AddLabel(const char* name, int32_t ip)
{
    SymbolTableEntry* symbol = new SymbolTableEntry();
    symbol->name = _strdup(name);
    symbol->type = SymbolType::Label;
    symbol->ip = ip;

    if (declaration_queue) {
        SymbolTableEntry* entry = declaration_queue;
        while (entry->next) {
            if (strcmp(name, entry->name) == 0) {
                std::string message = "Label \"";
                message += name;
                message += "\" is already declared in this scope";
                throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
            }
            entry = entry->next;
        }
        entry->next = symbol;
    } else {
        declaration_queue = symbol;
    }
}

void Compiler::AddStaticVariable(SymbolType type, const char* name)
{
    SymbolTableEntry* entry = AddSymbol(name, type, ReturnSymbolType::Unknown,
        ExpressionType::Variable, 0, GetSymbolTypeSize(type), 0, nullptr);
}

void Compiler::AddFunction(char* name, ReturnSymbolType return_type)
{

	int32_t ip = function_ip;
	function_ip = NextIp();

	Log("P: Found function definition \"" << name << "\" (" << parameter_count << " parameters) at " << ip);

	uint32_t offset_internal = 0;

	if (strcmp(name, EntryPointName) == 0) {
		// Entry point found
		if (parameter_count != 0) {
			throw CompilerException(CompilerExceptionSource::Declaration, "Entry point must have zero parameters", yylineno, -1);
		}
		if (return_type != ReturnSymbolType::Uint8) {
			throw CompilerException(CompilerExceptionSource::Declaration, "Entry point must return \"uint8\" value", yylineno, -1);
		}

		// Collect all variables used in the function
		SymbolTableEntry* current = declaration_queue;
		while (current) {
			int32_t size = current->offset_or_size;

			Log(" - " << current->name << " (" << size << " bytes)");

			AddSymbol(current->name, current->type, current->return_type,
				current->expression_type, current->ip, offset_internal, 0, name);

			offset_internal += size;
			offset_global += size;

			current = current->next;
		}

		SymbolTableEntry* entry = AddSymbol(name, SymbolType::EntryPoint, return_type,
			ExpressionType::None, ip, offset_internal, 0, nullptr);

		// Move entry point to the beginning of the list
		if (symbol_table != entry) {
			SymbolTableEntry* entry_before = symbol_table;
			while (entry_before->next != entry) {
				entry_before = entry_before->next;
			}

			entry_before->next = entry->next;
			entry->next = symbol_table;
			symbol_table = entry;
		}

		ReleaseDeclarationQueue();
		return;
	}

	// Find function prototype
	SymbolTableEntry* prototype = symbol_table;
	while (prototype) {
		if (prototype->type == SymbolType::FunctionPrototype && strcmp(name, prototype->name) == 0) {
			break;
		}

		prototype = prototype->next;
	}

	if (prototype) {
		if ((!declaration_queue && parameter_count != 0) || prototype->parameter != parameter_count) {
			std::string message = "Parameter count does not match for function \"";
			message += name;
			message += "\"";
			throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
		}

		// Promote the prototype to complete function
		prototype->type = SymbolType::Function;
		prototype->ip = ip;

		// Collect all function parameters
		SymbolTableEntry* current = symbol_table;
		for (uint16_t i = 0; i < parameter_count; i++) {
			while (!current->parent) {
				current = current->next;
				if (current->parent && strcmp(current->parent, name) == 0) {
					// Parameter found
					break;
				}
			}

			if (current->type != declaration_queue->type) {
				std::string message = "Parameter \"";
				message += current->name;
				message += "\" type does not match for function \"";
				message += name;
				message += "\"";
				throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
			}

			int32_t size = current->offset_or_size;

			Log(" - " << current->name << " (" << size << " bytes) at " << offset_internal);

			current->offset_or_size = offset_internal;

			offset_internal += size;
			offset_global += size;

			// Remove parameter from the queue
			SymbolTableEntry* remove = declaration_queue;
			declaration_queue = declaration_queue->next;
			delete remove;

			current = current->next;
		}

		// Collect all variables used in the function
		current = declaration_queue;
		while (current) {
			int32_t size = current->offset_or_size;

			Log(" - " << current->name << " (" << size << " bytes)");

			AddSymbol(current->name, current->type, current->return_type,
				current->expression_type, current->ip, offset_internal, 0, name);

			offset_internal += size;
			offset_global += size;

			current = current->next;
		}

		// Size of all parameters and variables
		prototype->offset_or_size = offset_internal;

		ReleaseDeclarationQueue();
		return;
	}

	// Prototype was not defined yet
	if (!declaration_queue && parameter_count != 0) {
		std::string message = "Parameter count does not match for function \"";
		message += name;
		message += "\"";
		throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
	}

	// Collect all function parameters and used variables
	uint16_t parameter_current = 0;
	SymbolTableEntry* current = declaration_queue;
	while (current) {
		int32_t size = current->offset_or_size;

		Log(" - " << current->name << " (" << size << " bytes) at " << offset_internal);

		if (parameter_current < parameter_count) {
			parameter_current++;
			current->parameter = parameter_current;
		}
		else {
			current->parameter = 0;
		}

		AddSymbol(current->name, current->type, current->return_type,
			current->expression_type, current->ip, offset_internal, current->parameter, name);

		offset_internal += size;
		offset_global += size;

		current = current->next;
	}

	AddSymbol(name, SymbolType::Function, return_type,
		ExpressionType::None, ip, offset_internal, parameter_count, nullptr);

	ReleaseDeclarationQueue();
}

void Compiler::AddFunctionPrototype(char* name, ReturnSymbolType return_type)
{
	Log("P: Found function prototype \"" << name << "\" (" << parameter_count << " parameters)");

	if (strcmp(name, EntryPointName) == 0) {
		throw CompilerException(CompilerExceptionSource::Declaration, "Prototype for entry point is not allowed", yylineno, -1);
	}
	if (!declaration_queue && parameter_count != 0) {
		throw CompilerException(CompilerExceptionSource::Declaration, "Parameter count does not match", yylineno, -1);
	}

	// Check if the function with the same name is already declared
	{
		SymbolTableEntry* current = symbol_table;
		while (current) {
			if ((current->type == SymbolType::FunctionPrototype || current->type == SymbolType::Function || current->type == SymbolType::EntryPoint) && strcmp(name, current->name) == 0) {
				std::string message = "Duplicate function definition for \"";
				message += current->name;
				message += "\"";
				throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
			}

			current = current->next;
		}
	}

	AddSymbol(name, SymbolType::FunctionPrototype, return_type,
		ExpressionType::None, 0, 0, parameter_count, nullptr);

	// Collect all function parameters
	{
		uint16_t parameter_current = 0;
		SymbolTableEntry* current = declaration_queue;
		while (current) {
			parameter_current++;

			AddSymbol(current->name, current->type, current->return_type,
				current->expression_type, current->ip, current->offset_or_size, parameter_current, name);

			current = current->next;
		}
	}

	ReleaseDeclarationQueue();
}

void Compiler::PrepareForCall(const char* name, SymbolTableEntry* call_parameters, int32_t parameter_count)
{
	// Find function by its name
	SymbolTableEntry* current = symbol_table;
	while (current) {
		if ((current->type == SymbolType::Function || current->type == SymbolType::FunctionPrototype) && strcmp(name, current->name) == 0) {
			break;
		}

		current = current->next;
	}

	if (!current) {
		std::string message = "Cannot call function \"";
		message += name;
		message += "\", because it was not declared";
		throw CompilerException(CompilerExceptionSource::Statement, message, yylineno, -1);
	}

	current = symbol_table;
	int32_t parameters_found = 0;

	do {
		// Find parameter description
		while (current) {
			if (current->parent && strcmp(name, current->parent) == 0 && current->parameter != 0) {
				break;
			}

			current = current->next;
		}

		if (!call_parameters || !current) {
			// No parameters found
			if (parameter_count != 0 || parameters_found != 0) {
				std::string message = "Cannot call function \"";
				message += name;
				message += "\" because of parameter count mismatch";
				throw CompilerException(CompilerExceptionSource::Statement, message, yylineno, -1);
			}

			return;
		}

		// ToDo: Correct ExpressionType
		if (!CanImplicitCast(current->type, call_parameters->type, call_parameters->expression_type)) {
			std::string message = "Cannot call function \"";
			message += name;
			message += "\" because of parameter \"";
			message += current->name;
			message += "\" type mismatch";
			throw CompilerException(CompilerExceptionSource::Statement, message, yylineno, -1);
		}

		// Add required parameter to stream
		char buffer[50];
		sprintf_s(buffer, "push %s", call_parameters->name);
		InstructionEntry* i = AddToStream(InstructionType::Push, buffer);
		i->push_statement.symbol = call_parameters;

		current = current->next;

		// Remove processed entry from the list
		//SymbolTableEntry* temp = call_parameters;
		call_parameters = call_parameters->next;
		//delete temp;

		parameters_found++;
	} while (parameters_found < parameter_count);
}

SymbolTableEntry* Compiler::GetParameter(const char* name)
{
	// Search in function-local variable list
	SymbolTableEntry* current = declaration_queue;
	while (current) {
		if (strcmp(name, current->name) == 0) {
			return current;
		}

		current = current->next;
	}

	if (!current) {
		// Search in static variable list
		current = symbol_table;
		while (current) {
			if (!current->parent &&
				current->type != SymbolType::Function &&
				current->type != SymbolType::FunctionPrototype &&
				current->type != SymbolType::EntryPoint &&
				strcmp(name, current->name) == 0) {
				return current;
			}

			current = current->next;
		}
	}

	return nullptr;
}

SymbolTableEntry* Compiler::GetFunction(const char* name)
{
	SymbolTableEntry* current = symbol_table;
	while (current) {
		if ((current->type == SymbolType::Function || current->type == SymbolType::FunctionPrototype) && strcmp(name, current->name) == 0) {
			return current;
		}

		current = current->next;
	}

	return nullptr;
}
