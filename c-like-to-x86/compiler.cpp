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