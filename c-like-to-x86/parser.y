%{

#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

void yyerror(const char* s);

extern int yylex();
extern int yyparse();
extern FILE* yyin;
extern int yylineno;
extern char *yytext;

Compiler compiler;

%}
%token CONST STATIC VOID BOOL UINT8 UINT16 UINT32 STRING CONSTANT IDENTIFIER
%token IF ELSE RETURN DO WHILE FOR
%token EQUAL NOT_EQUAL GREATER_OR_EQUAL LESS_OR_EQUAL
%token SHIFT_LEFT SHIFT_RIGHT
%token LOG_AND LOG_OR

%right '='
%left EQUAL NOT_EQUAL LESS_OR_EQUAL GREATER_OR_EQUAL '<' '>'
%left SHIFT_LEFT SHIFT_RIGHT
%left LOG_AND LOG_OR
%left '+' '-' '*' '/' '%'

%start program_head

%%

program_head
    : program
		{
		}
    ;

program
    : jump_marker function
        {
            Log("P: Processing single function");

        }
    | program function
        {
            Log("P: Processing function in function list");

        }
    ;


// Parts of function declaration
function
    : return_type id '(' parameter_list ')' ';'
        {
        }
    | return_type id '(' parameter_list ')' function_body
        {
        }
	| static_declaration_list
		{
		}
    ;

function_body
    : '{' statement_list  '}'
        {
            Log("P: Found function body");
        }
    ;

parameter_list
    : declaration_type id
        {
            Log("P: Found parameter");
        }
    | parameter_list ',' declaration_type id
        {

            Log("P: Found parameter list (end)");
        }
    | VOID
        {
            Log("P: Found VOID parameter");
        }
    |
        {
            Log("P: Found no parameter");
        }
    ;

// Types that can be used for variable declarations and function parameters
declaration_type
	: BOOL
        {
            $$ = SymbolType::Bool;

			Log("P: Found BOOL declaration_type");
        }
	| UINT8
        {
            $$ = SymbolType::Uint8;

			Log("P: Found UINT8 declaration_type");
        }
	| UINT16
        {
            $$ = SymbolType::Uint16;

			Log("P: Found UINT16 declaration_type");
        }
	| UINT32
        {
            $$ = SymbolType::Uint32;

			Log("P: Found UINT32 declaration_type");
        }
	| STRING
        {
            $$ = SymbolType::String;

			Log("P: Found STRING declaration_type");
        }
    ;

// Types that can be used for function return value
return_type
	: BOOL
        {
            $$ = ReturnSymbolType::Bool;

            Log("P: Found BOOL return_type");
        }
	| UINT8
        {
            $$ = ReturnSymbolType::Uint8;

            Log("P: Found UINT8 return_type");
        }
	| UINT16
        {
            $$ = ReturnSymbolType::Uint16;

            Log("P: Found UINT16 return_type");
        }
	| UINT32
        {
            $$ = ReturnSymbolType::Uint32;

            Log("P: Found UINT32 return_type");
        }
	| STRING
        {
            $$ = ReturnSymbolType::String;

            Log("P: Found STRING return_type");
        }
	| VOID
        {
            $$ = ReturnSymbolType::Void;

            Log("P: Found VOID return_type");
        }
    ;



%%

int __cdecl wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	return compiler.OnRun(argc, argv);
}

void yyerror(const char* s)
{
	compiler.OnError(s);
}

