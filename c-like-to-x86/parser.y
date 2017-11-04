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

%token INT BOOL VOID CONSTANT IDENTIFIER
%token IF ELSE RETURN DO WHILE FOR
%token EQUAL NOT_EQUAL GREATER_OR_EQUAL LESS_OR_EQUAL
%token SHIFT_LEFT SHIFT_RIGHT
%token LOG_AND LOG_OR

%right '='
%left EQUAL NOT_EQUAL LESS_OR_EQUAL GREATER_OR_EQUAL '<' '>'
%left SHIFT_LEFT SHIFT_RIGHT
%left LOG_AND LOG_OR
%left '+' '-' '*' '/' '%'

%%

%%

int __cdecl wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	return compiler.OnRun(argc, argv);
}

void yyerror(const char* s)
{
	compiler.OnError(s);
}