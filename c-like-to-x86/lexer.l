%option noyywrap
%option yylineno
%option case-insensitive
%option nostdinit

%{
#include <stdio.h>
#include <stdbool.h>

#include "Compiler.h"
#include "parser.tab.h"

#define YY_DECL int yylex()
#define YY_USER_ACTION									\
	yylloc.first_line = yylloc.last_line = yylineno;	\
    yylloc.first_column = yycolumn;						\
	yylloc.last_column = yycolumn + yyleng - 1;			\
    yycolumn += yyleng;

int yycolumn = 1;
bool allow_unary = false;

%}

NEWLINE (\n|\r\n)
WHITESPACE (" "|\t)

LINE_COMMENT ("//".*{NEWLINE})
BLOCK_COMMENT ("/*".*"*/")

PREPROCESSOR ("#".*{NEWLINE})

INTEGER (0|[1-9][0-9]*)
BOOL_TRUE true
BOOL_FALSE false
IDENTIFIER ([a-zA-Z_][a-zA-Z0-9_]*)

%%

({WHITESPACE}+) {
	// Ignore whitespaces
}

({NEWLINE}+) {
	// Ignore newlines
	yycolumn = 1;
}

({LINE_COMMENT}|{BLOCK_COMMENT}) {
	// Ignore comments
}

{PREPROCESSOR} {
	// ToDo
	Log("L: Found preprocessor directive");
}

^"-" {
	Log("L: Found unary minus");

	allow_unary = false;
	return U_MINUS;
}

^"+" {
	Log("L: Found unary plus");

	allow_unary = false;
	return U_PLUS;
}

("("|")"|"{"|"}"|"<"|">"|"="|";"|","|"!"|":") {
	Log("L: Found " << yytext[0]);

	allow_unary = true;
	return yytext[0];
}

("/"|"*"|"%") {
	Log("L: Found " << yytext[0]);

	allow_unary = true;
	return yytext[0];
}

"-" {
	if (allow_unary) {
		Log("L: Found unary minus");

		allow_unary = false;
		return U_MINUS;
	} else {
		Log("L: Found minus");

		return yytext[0];
	}
}

"+" {
	if (allow_unary) {
		Log("L: Found unary plus");

		allow_unary = false;
		return U_PLUS;
	} else {
		Log("L: Found plus");

		return yytext[0];
	}
}

"const" {
	Log("L: Found \"const\"");

	return CONST;
}

"static" {
	Log("L: Found \"static\"");

	return STATIC;
}

"void" {
	Log("L: Found \"void\"");

	return VOID;
}

"bool" {
	Log("L: Found \"bool\"");

	return BOOL;
}

"uint8" {
	Log("L: Found \"uint8\"");

	return UINT8;
}

"uint16" {
	Log("L: Found \"uint16\"");

	return UINT16;
}

"uint32" {
	Log("L: Found \"uint32\"");

	return UINT32;
}

"string" {
	Log("L: Found \"string\"");

	return STRING;
}

"if" {
	Log("L: Found \"if\"");

	return IF;
}

"else" {
	Log("L: Found \"else\"");

	return ELSE;
}

"return" {
	Log("L: Found \"return\"");

	return RETURN;
}

"do" {
	Log("L: Found \"do\"");

	return DO;
}

"while" {
	Log("L: Found \"while\"");

	return WHILE;
}

"for" {
	Log("L: Found \"for\"");

	return FOR;
}

"goto" {
	Log("L: Found \"goto\"");

	return GOTO;
}

"++" {
	Log("L: Found ++");

	return INC_OP;
}

"--" {
	Log("L: Found --");

	return DEC_OP;
}

"==" {
	Log("L: Found ==");

	return EQUAL;
}

"!=" {
	Log("L: Found !=");

	return NOT_EQUAL;
}

">=" {
	Log("L: Found >=");

	return GREATER_OR_EQUAL;
}

"<=" {
	Log("L: Found <=");

	return LESS_OR_EQUAL;
}

"<<" {
	Log("L: Found <<");

	return SHIFT_LEFT;
}

">>" {
	Log("L: Found >>");

	return SHIFT_RIGHT;
}

"&&" {
	Log("L: Found &&");

	return LOG_AND;
}

"||" {
	Log("L: Found ||");

	return LOG_OR;
}

{INTEGER} {
	Log("L: Found integer constant \"" << yytext << "\"");

	yylval.expression.value = _strdup(yytext);
	yylval.expression.expression_type = ExpressionType::Constant;
	yylval.expression.type = SymbolType::Uint32;
	allow_unary = false;
	return CONSTANT;
}

{BOOL_TRUE} {
	Log("L: Found bool constant \"true\"");

	yylval.expression.value = _strdup("1");
	yylval.expression.expression_type = ExpressionType::Constant;
	yylval.expression.type = SymbolType::Bool;
	allow_unary = false;
	return CONSTANT;
}

{BOOL_FALSE} {
	Log("L: Found bool constant \"false\"");

	yylval.expression.value = _strdup("0");
	yylval.expression.expression_type = ExpressionType::Constant;
	yylval.expression.type = SymbolType::Bool;
	allow_unary = false;
	return CONSTANT;
}

{IDENTIFIER} {
	Log("L: Found identifier \"" << yytext << "\"");

	yylval.string = _strdup(yytext);
	allow_unary = false;
	return IDENTIFIER;
}

%%
