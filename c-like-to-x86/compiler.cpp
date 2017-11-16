#include "Compiler.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <string>

#include "DosExeEmitter.h"

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
    if (argc < 2) {
        std::wcout << "You must specify at least output filename!";
        return EXIT_FAILURE;
    }

    wchar_t* output_filename;

    // Open input file
    bool is_file;

    if (argc >= 3) {
        errno_t err = _wfopen_s(&yyin, argv[1], L"rb");
        if (err) {
            wchar_t error[200];
            _wcserror_s(error, err);
            std::wcout << "Error while opening input file: " << error;
            return EXIT_FAILURE;
        }

        is_file = true;

        output_filename = argv[2];
    } else {
        yyin = stdin;
        is_file = false;

        output_filename = argv[1];
    }

    // Open output files
    FILE* outputExe;
    errno_t err = _wfopen_s(&outputExe, output_filename, L"wb");
    if (err) {
        wchar_t error[200];
        _wcserror_s(error, err);
        std::wcout << "Error while creating executable: " << error;
        return EXIT_FAILURE;
    }

    FILE* outputAsm;
    err = fopen_s(&outputAsm, "output.asm", "wb");
    if (err) {
        wchar_t error[200];
        _wcserror_s(error, err);
        std::wcout << "Error while creating assembly listing: " << error;
        return EXIT_FAILURE;
    }

    // Declare all shared functions
    DeclareSharedFunctions();

    // Parse input file
    try {
        Log("Parsing source code...");

        do {
            yyparse();
        } while (!feof(yyin));

        Log("Sorting symbol table...");

        SortSymbolTable();

        Log("Creating executable file...");

        // Parsing was successful, generate output files
        {
            DosExeEmitter emitter(this);
            emitter.EmitMzHeader();

            Log("Compiling intermediate code to x86 instructions...");

            emitter.EmitInstructions(instruction_stream_head, symbol_table);
            emitter.EmitSharedFunctions(symbol_table);
            emitter.EmitStaticData();
            emitter.FixMzHeader(instruction_stream_head, stack_size);
            emitter.Save(outputExe);

            CreateDebugOutput(outputAsm);
        }

        Log("Done!");
    } catch (CompilerException& ex) {
        // Input file can't be parsed/compiled
        // Cleanup
        if (is_file) {
            fclose(yyin);
        }

        fclose(outputExe);
        fclose(outputAsm);

        // Show error message
        int32_t line = ex.GetLine();
        if (line >= 0) {
            std::cout << "[" << line;

            int32_t column = ex.GetColumn();
            if (column >= 0) {
                std::cout << ":" << column;
            }

            std::cout << "] ";
        }

        switch (ex.GetSource()) {
            case CompilerExceptionSource::Syntax: std::cout << "Syntax Error: "; break;
            case CompilerExceptionSource::Declaration: std::cout << "Declaration Error: "; break;
            case CompilerExceptionSource::Statement: std::cout << "Statement Error: "; break;

            default: std::cout << "Error: "; break;
        }

        std::cout << ex.what() << "\r\n";
        return EXIT_FAILURE;
    }

    if (is_file) {
        fclose(yyin);
    }

    fclose(outputExe);
    fclose(outputAsm);

    ReleaseAll();

    return EXIT_SUCCESS;
}

void Compiler::CreateDebugOutput(FILE* output_file)
{
    // Generate symbol table
    {
        fprintf(output_file, "Symbols\r\n"
            "___________________________________________________________________________________________\r\n"
            ""
            "Name            Type         Return    IP       Offset/Size  Param.  Exp. Type  Parent\r\n"
            "___________________________________________________________________________________________\r\n");
            
        SymbolTableEntry* current = symbol_table;

        while (current) {
            fprintf(output_file,
                "%-15s %-12s %-9s %-8lu %-12lu %-7lu %-6s %-3s %s\r\n",
                current->name,
                SymbolTypeToString(current->type),
                ReturnSymbolTypeToString(current->return_type),
                current->ip,
                current->offset_or_size,
                current->parameter,
                ExpressionTypeToString(current->exp_type),
                (current->is_temp ? "[T]" : ""),
                current->parent == nullptr ? "-" : current->parent);

            current = current->next;
        }
    }

    // Generate sequential code
    {
        fprintf(output_file, "\r\n\r\nStream\r\n"
            "___________________________________________________________________________________________\r\n\r\n");

        SymbolTableEntry* parent = nullptr;
        InstructionEntry* current = instruction_stream_head;
        uint64_t ip = 0;

        while (current) {

            SymbolTableEntry* symbol = symbol_table;

            while (symbol) {
                if ((symbol->type == SymbolType::Function || symbol->type == SymbolType::EntryPoint) && symbol->ip == ip) {
                    parent = parent;
                }

                if ((symbol->type == SymbolType::Function || symbol->type == SymbolType::EntryPoint || symbol->type == SymbolType::Label) && symbol->ip == ip) {
                    fprintf(output_file, "%s:\r\n", symbol->name);
                }

                symbol = symbol->next;
            }

            if (current->goto_ip == -1) {
                fprintf(output_file, "    %-5llu %s\r\n", ip, current->content);
            } else {
                fprintf(output_file, "    %-5llu %s %d\r\n", ip, current->content, current->goto_ip);
            }

            current = current->next;
            ip++;
        }
    }
}

void Compiler::ParseCompilerDirective(char* directive)
{
    int32_t name_length = 0;
    char* param = directive;
    while (*param && *param != ' ') {
        param++;
        name_length++;
    }

    if (*param == ' ') {
        param++;

        // Parameter provided
        if (memcmp(directive, "#stack", 6) == 0) {
            stack_size = atoi(param);
            return;
        }
    }

    Log("Compiler directive \"" << directive << "\" cannot be resolved");
}

InstructionEntry* Compiler::AddToStream(InstructionType type, char* code)
{
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
            // ToDo: Remove this
            list->entry->goto_ip = new_ip;

            if (list->entry->type == InstructionType::Goto) {
                list->entry->goto_statement.ip = new_ip;
            } else if (list->entry->type == InstructionType::If) {
                list->entry->if_statement.ip = new_ip;
            } else {
                // This type cannot be backpatched...
                Log("Trying to backpatch unsupported instruction");

                ThrowOnUnreachableCode();
            }
        }

        // Release entry in backpatch linked list
        BackpatchList* current = list;
        list = list->next;
        delete current;
    }
}


SymbolTableEntry* Compiler::ToDeclarationList(SymbolType type, const char* name, ExpressionType exp_type)
{
    SymbolTableEntry* symbol = new SymbolTableEntry();
    symbol->name = _strdup(name);
    symbol->type = type;
    symbol->exp_type = exp_type;
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
    } else {
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
    } else {
        declaration_queue = symbol;
    }
}

SymbolTableEntry* Compiler::ToCallParameterList(SymbolTableEntry* list, SymbolType type, const char* name, ExpressionType exp_type)
{
    SymbolTableEntry* symbol = new SymbolTableEntry();
    symbol->name = _strdup(name);
    symbol->type = type;
    symbol->exp_type = exp_type;

    if (list) {
        SymbolTableEntry* entry = list;
        while (entry->next) {
            entry = entry->next;
        }
        entry->next = symbol;
        return list;
    } else {
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
        ExpressionType::Variable, 0, GetSymbolTypeSize(type), 0, nullptr, false);
}

void Compiler::AddFunction(char* name, ReturnSymbolType return_type)
{
    // Check, if the function is not defined yet
    {
        SymbolTableEntry* current = symbol_table;
        while (current) {
            if ((current->type == SymbolType::Function ||
                 current->type == SymbolType::EntryPoint ||
                 current->type == SymbolType::SharedFunction) &&
                strcmp(name, current->name) == 0) {

                std::string message = "Function \"";
                message += name;
                message += "\" is already defined";
                throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
            }

            current = current->next;
        }
    }

    int32_t ip = function_ip;
    function_ip = NextIp();

    //Log("P: Found function definition \"" << name << "\" (" << parameter_count << " parameters) at " << ip);

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

            //Log(" - " << current->name << " (" << size << " bytes)");

            AddSymbol(current->name, current->type, current->return_type,
                current->exp_type, current->ip, offset_internal, 0, name, current->is_temp);

            offset_internal += size;
            offset_global += size;

            current = current->next;
        }

        SymbolTableEntry* entry = AddSymbol(name, SymbolType::EntryPoint, return_type,
            ExpressionType::None, ip, offset_internal, 0, nullptr, false);

        /*
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
        */

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
            while (!current->parent || strcmp(current->parent, name) != 0) {
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

            //Log(" - " << current->name << " (" << size << " bytes) at " << offset_internal);

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

            //Log(" - " << current->name << " (" << size << " bytes)");

            AddSymbol(current->name, current->type, current->return_type,
                current->exp_type, current->ip, offset_internal, 0, name, current->is_temp);

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

        //Log(" - " << current->name << " (" << size << " bytes) at " << offset_internal);

        if (parameter_current < parameter_count) {
            parameter_current++;
            current->parameter = parameter_current;
        } else {
            current->parameter = 0;
        }

        AddSymbol(current->name, current->type, current->return_type,
            current->exp_type, current->ip, offset_internal, current->parameter, name, current->is_temp);

        offset_internal += size;
        offset_global += size;

        current = current->next;
    }

    AddSymbol(name, SymbolType::Function, return_type,
        ExpressionType::None, ip, offset_internal, parameter_count, nullptr, false);

    ReleaseDeclarationQueue();
}

void Compiler::AddFunctionPrototype(char* name, ReturnSymbolType return_type)
{
    //Log("P: Found function prototype \"" << name << "\" (" << parameter_count << " parameters)");

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
            if ((current->type == SymbolType::FunctionPrototype ||
                 current->type == SymbolType::Function ||
                 current->type == SymbolType::EntryPoint ||
                 current->type == SymbolType::SharedFunction) &&
                strcmp(name, current->name) == 0) {

                std::string message = "Duplicate function definition for \"";
                message += current->name;
                message += "\"";
                throw CompilerException(CompilerExceptionSource::Declaration, message, yylineno, -1);
            }

            current = current->next;
        }
    }

    AddSymbol(name, SymbolType::FunctionPrototype, return_type,
        ExpressionType::None, 0, 0, parameter_count, nullptr, false);

    // Collect all function parameters
    {
        uint16_t parameter_current = 0;
        SymbolTableEntry* current = declaration_queue;
        while (current) {
            parameter_current++;

            AddSymbol(current->name, current->type, current->return_type,
                current->exp_type, current->ip, current->offset_or_size, parameter_current, name, current->is_temp);

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
        if ((current->type == SymbolType::Function ||
             current->type == SymbolType::FunctionPrototype ||
             current->type == SymbolType::SharedFunction) &&
            strcmp(name, current->name) == 0) {

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

    if (current->type == SymbolType::SharedFunction) {
        // IP of shared function means reference count
        current->ip++;
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
        if (!CanImplicitCast(current->type, call_parameters->type, call_parameters->exp_type)) {
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
                current->type != SymbolType::SharedFunction &&
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
        if ((current->type == SymbolType::Function ||
             current->type == SymbolType::FunctionPrototype ||
             current->type == SymbolType::SharedFunction) &&
            strcmp(name, current->name) == 0) {

            return current;
        }

        current = current->next;
    }

    return nullptr;
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
    if (a == SymbolType::String || b == SymbolType::String)
        return SymbolType::Unknown;
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
        case SymbolType::String: {
            var_count_string++;
            sprintf_s(buffer, "__s_%d", var_count_uint32);
            break;
        }

        default: ThrowOnUnreachableCode();
    }

    SymbolTableEntry* decl = ToDeclarationList(type, buffer, ExpressionType::Variable);
    decl->is_temp = true;
    return decl;
}



SymbolTableEntry* Compiler::AddSymbol(const char* name, SymbolType type, ReturnSymbolType return_type,
    ExpressionType exp_type, int32_t ip, int32_t offset_or_size, int32_t parameter, const char* parent, bool is_temp)
{
    if (!name || strlen(name) == 0) {
        throw CompilerException(CompilerExceptionSource::Declaration, "Symbol name must not be empty", yylineno, -1);
    }

    SymbolTableEntry* symbol = new SymbolTableEntry();
    symbol->name = _strdup(name);
    symbol->type = type;
    symbol->return_type = return_type;
    symbol->exp_type = exp_type;
    symbol->ip = ip;
    symbol->offset_or_size = offset_or_size;
    symbol->parameter = parameter;
    symbol->parent = (parent == nullptr ? nullptr : _strdup(parent));
    symbol->is_temp = is_temp;

    // Add it to the symbol table
    SymbolTableEntry* tail = symbol_table;
    if (tail) {
        while (tail->next) {
            tail = tail->next;
        }

        tail->next = symbol;
    } else {
        symbol_table = symbol;
    }

    return symbol;
}

const char* Compiler::SymbolTypeToString(SymbolType type)
{
    switch (type) {
        case SymbolType::Function: return "Function";
        case SymbolType::FunctionPrototype: return "Prototype";
        case SymbolType::EntryPoint: return "EntryPoint";
        case SymbolType::SharedFunction: return "SharedFun.";

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

        case SymbolType::String: return 2; // 16-bit pointer

        default: ThrowOnUnreachableCode();
    }
}

int32_t Compiler::GetReturnSymbolTypeSize(ReturnSymbolType type)
{
    switch (type) {
        case ReturnSymbolType::Bool: return 1;
        case ReturnSymbolType::Uint8: return 1;
        case ReturnSymbolType::Uint16: return 2;
        case ReturnSymbolType::Uint32: return 4;

        case ReturnSymbolType::String: return 2; // 16-bit pointer

        default: ThrowOnUnreachableCode();
    }
}

void Compiler::IncreaseScope(ScopeType type)
{
    switch (type) {
        case ScopeType::Break: {
            break_scope++;
            break_list.push_back(nullptr);
            break;
        }

        case ScopeType::Continue: {
            continue_scope++;
            continue_list.push_back(nullptr);
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

void Compiler::BackpatchScope(ScopeType type, int32_t new_ip)
{
    switch (type) {
        case ScopeType::Break: {
            BackpatchStream(break_list[break_scope], new_ip);

            break_list[break_scope] = nullptr;
            break_scope--;
            break;
        }

        case ScopeType::Continue: {
            BackpatchStream(continue_list[continue_scope], new_ip);

            continue_list[continue_scope] = nullptr;
            continue_scope--;
            break;
        }

        default: ThrowOnUnreachableCode();
    }
}

bool Compiler::AddToScopeList(ScopeType type, BackpatchList* backpatch)
{
    switch (type) {
        case ScopeType::Break: {
            if (break_scope < 0) {
                return false;
            }

            size_t list_size = break_list.size();
            if (list_size == 0 || list_size == break_scope - 1) {
                break_list.push_back(backpatch);
            } else {
                break_list[break_scope] = MergeLists(backpatch, break_list[break_scope]);
            }
            break;
        }

        case ScopeType::Continue: {
            if (continue_scope < 0) {
                return false;
            }

            size_t list_size = continue_list.size();
            if (list_size == 0 || list_size == continue_scope - 1) {
                continue_list.push_back(backpatch);
            } else {
                continue_list[continue_scope] = MergeLists(backpatch, continue_list[continue_scope]);
            }
            break;
        }

        default: ThrowOnUnreachableCode();
    }

    return true;
}

void Compiler::ReleaseDeclarationQueue()
{
    while (declaration_queue) {
        SymbolTableEntry* current = declaration_queue;
        declaration_queue = declaration_queue->next;
        //free(current->name);
        delete current;
    }

    parameter_count = 0;
}

void Compiler::ReleaseAll()
{
    ReleaseDeclarationQueue();

    while (instruction_stream_head) {
        InstructionEntry* current = instruction_stream_head;
        instruction_stream_head = instruction_stream_head->next;
        free(current->content);
        delete current;
    }

    instruction_stream_tail = nullptr;

    while (symbol_table) {
        SymbolTableEntry* current = symbol_table;
        symbol_table = symbol_table->next;
        free(current->name);
        free(current->parent);
        delete current;
    }
}

void Compiler::SortSymbolTable()
{
    if (!symbol_table) {
        return;
    }

    // Fix IP of first function
    SymbolTableEntry* symbol = symbol_table;
    while (symbol) {
        if (!symbol->parent &&
            (symbol->type == SymbolType::Function ||
             symbol->type == SymbolType::EntryPoint)) {

            if (symbol->ip == 0) {
                symbol->ip = 1;
                break;
            }
        }

        symbol = symbol->next;
    }

    // Find last entry of the symbol table
    SymbolTableEntry* end = symbol_table;
    while (end->next) {
        end = end->next;
    }

    // Move all static variables to the end of the symbol table
    SymbolTableEntry* prev = nullptr;
    SymbolTableEntry* static_start = nullptr;
    SymbolTableEntry* current = symbol_table;
    while (current && current != static_start) {
        if (!current->parent &&
            current->type != SymbolType::Function &&
            current->type != SymbolType::FunctionPrototype &&
            current->type != SymbolType::EntryPoint &&
            current->type != SymbolType::SharedFunction) {

            if (prev) {
                prev->next = current->next;
            } else {
                symbol_table = current->next;
            }

            SymbolTableEntry* next = current->next;

            end->next = current;
            current->next = nullptr;

            if (!static_start) {
                static_start = current;
            }

            end = current;
            current = next;
        } else {
            prev = current;
            current = current->next;
        }
    }

    // Convert size to offset
    uint32_t offset_internal = 0;

    current = static_start;
    while (current) {
        int32_t size = current->offset_or_size;

        current->offset_or_size = offset_internal;
        current->ip = offset_internal + function_ip;

        offset_internal += size;

        current = current->next;
    }
}

void Compiler::DeclareSharedFunctions()
{
    // void PrintUint32(uint32 value);
    AddSymbol("PrintUint32", SymbolType::SharedFunction, ReturnSymbolType::Void,
        ExpressionType::None, 0, 0, 1, nullptr, false);

    AddSymbol("value", SymbolType::Uint32, ReturnSymbolType::Unknown,
        ExpressionType::None, 0, 0, 1, "PrintUint32", false);

    // void PrintString(string value);
    AddSymbol("PrintString", SymbolType::SharedFunction, ReturnSymbolType::Void,
        ExpressionType::None, 0, 0, 1, nullptr, false);

    AddSymbol("value", SymbolType::String, ReturnSymbolType::Unknown,
        ExpressionType::None, 0, 0, 1, "PrintString", false);

    // void PrintNewLine();
    AddSymbol("PrintNewLine", SymbolType::SharedFunction, ReturnSymbolType::Void,
        ExpressionType::None, 0, 0, 0, nullptr, false);

    // uint32 ReadUint32();
    AddSymbol("ReadUint32", SymbolType::SharedFunction, ReturnSymbolType::Uint32,
        ExpressionType::None, 0, 0, 0, nullptr, false);
}
