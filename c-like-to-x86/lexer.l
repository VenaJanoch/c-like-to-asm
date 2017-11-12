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
STRING (\".*\")
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
	LogVerbose("L: Found unary minus");

	allow_unary = false;
	return U_MINUS;
}

^"+" {
	LogVerbose("L: Found unary plus");

	allow_unary = false;
	return U_PLUS;
}

("("|")"|"{"|"}"|"<"|">"|"="|";"|","|"!"|":") {
	LogVerbose("L: Found " << yytext[0]);

	allow_unary = true;
	return yytext[0];
}

("/"|"*"|"%") {
	LogVerbose("L: Found " << yytext[0]);

	allow_unary = true;
	return yytext[0];
}

"-" {
	if (allow_unary) {
		LogVerbose("L: Found unary minus");

		allow_unary = false;
		return U_MINUS;
	} else {
		LogVerbose("L: Found minus");

		return yytext[0];
	}
}

"+" {
	if (allow_unary) {
		LogVerbose("L: Found unary plus");

		allow_unary = false;
		return U_PLUS;
	} else {
		LogVerbose("L: Found plus");

		return yytext[0];
	}
}

"const" {
	LogVerbose("L: Found \"const\"");

	return CONST;
}

"static" {
	LogVerbose("L: Found \"static\"");

	return STATIC;
}

"void" {
	LogVerbose("L: Found \"void\"");

	return VOID;
}

"bool" {
	LogVerbose("L: Found \"bool\"");

	return BOOL;
}

"uint8" {
	LogVerbose("L: Found \"uint8\"");

	return UINT8;
}

"uint16" {
	LogVerbose("L: Found \"uint16\"");

	return UINT16;
}

"uint32" {
	LogVerbose("L: Found \"uint32\"");

	return UINT32;
}

"string" {
	LogVerbose("L: Found \"string\"");

	return STRING;
}

"if" {
	LogVerbose("L: Found \"if\"");

	return IF;
}

"else" {
	LogVerbose("L: Found \"else\"");

	return ELSE;
}

"return" {
	LogVerbose("L: Found \"return\"");

	return RETURN;
}

"do" {
	LogVerbose("L: Found \"do\"");

	return DO;
}

"while" {
	LogVerbose("L: Found \"while\"");

	return WHILE;
}

"for" {
	LogVerbose("L: Found \"for\"");

	return FOR;
}

"goto" {
	LogVerbose("L: Found \"goto\"");

	return GOTO;
}

"++" {
	LogVerbose("L: Found ++");

	return INC_OP;
}

"--" {
	LogVerbose("L: Found --");

	return DEC_OP;
}

"==" {
	LogVerbose("L: Found ==");

	return EQUAL;
}

"!=" {
	LogVerbose("L: Found !=");

	return NOT_EQUAL;
}

">=" {
	LogVerbose("L: Found >=");

	return GREATER_OR_EQUAL;
}

"<=" {
	LogVerbose("L: Found <=");

	return LESS_OR_EQUAL;
}

"<<" {
	LogVerbose("L: Found <<");

	return SHIFT_LEFT;
}

">>" {
	LogVerbose("L: Found >>");

	return SHIFT_RIGHT;
}

"&&" {
	LogVerbose("L: Found &&");

	return LOG_AND;
}

"||" {
	LogVerbose("L: Found ||");

	return LOG_OR;
}

{INTEGER} {
	LogVerbose("L: Found integer constant \"" << yytext << "\"");

	yylval.expression.value = _strdup(yytext);
	yylval.expression.expression_type = ExpressionType::Constant;

	int32_t value = atoi(yytext);
	if (value == (int8_t)value || value == (uint8_t)value) {
        yylval.expression.type = SymbolType::Uint8;
    } else if (value == (int16_t)value || value == (uint16_t)value) {
        yylval.expression.type = SymbolType::Uint16;
    } else {
        yylval.expression.type = SymbolType::Uint32;
    }

	allow_unary = false;
	return CONSTANT;
}

{BOOL_TRUE} {
	LogVerbose("L: Found bool constant \"true\"");

	yylval.expression.value = _strdup("1");
	yylval.expression.expression_type = ExpressionType::Constant;
	yylval.expression.type = SymbolType::Bool;
	allow_unary = false;
	return CONSTANT;
}

{BOOL_FALSE} {
	LogVerbose("L: Found bool constant \"false\"");

	yylval.expression.value = _strdup("0");
	yylval.expression.expression_type = ExpressionType::Constant;
	yylval.expression.type = SymbolType::Bool;
	allow_unary = false;
	return CONSTANT;
}

{STRING} {
	LogVerbose("L: Found string constant \"" << yytext << "\"");

	char* value = new char[yyleng - 1];
	memcpy(value, yytext + 1, yyleng - 2);
	value[yyleng - 2] = '\0';

	yylval.expression.value = value;
	yylval.expression.expression_type = ExpressionType::Constant;
    yylval.expression.type = SymbolType::String;

	return CONSTANT;
}

{IDENTIFIER} {
	LogVerbose("L: Found identifier \"" << yytext << "\"");

	yylval.string = _strdup(yytext);
	allow_unary = false;
	return IDENTIFIER;
}

%%
