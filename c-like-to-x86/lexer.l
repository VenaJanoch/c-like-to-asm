%option noyywrap
%option yylineno
%option case-insensitive

%{
#include <stdio.h>

#include "compiler.h"
#include "parser.tab.h"

#define YY_DECL int yylex()

%}

NEWLINE (\n|\r\n)
WHITESPACE (" "|\t)

SINGLELINE_COMMENT ("//".*{NEWLINE})
MULTILINE_COMMENT ("/*".*"*/")

INTEGER (0|[1-9][0-9]*)
BOOL_TRUE true
BOOL_FALSE false
IDENTIFIER ([a-zA-Z_][a-zA-Z0-9_]*)

%%

({NEWLINE}+|{WHITESPACE}+) {
	// Ignore whitespaces and newlines
}

({SINGLELINE_COMMENT}|{MULTILINE_COMMENT}) {
	// Ignore comments
}


("("|")"|"{"|"}"|"<"|">"|"="|";"|",") {
	return yytext[0];
}

("/"|"*"|"%") {
	return yytext[0];
}

"-" {
	return yytext[0];
}

"+" {
	return yytext[0];
}


"void" {
	return VOID;
}

"int" {
	return INT;
}

"bool" {
	return BOOL;
}


"if" {
	return IF;
}

"else" {
	return ELSE;
}

"return" {
	return RETURN;
}

"do" {
	return DO;
}

"while" {
	return WHILE;
}

"for" {
	return FOR;
}


"==" {
	return EQUAL;
}

"!=" {
	return NOT_EQUAL;
}

">=" {
	return GREATER_OR_EQUAL;
}

"<=" {
	return LESS_OR_EQUAL;
}

"&&" {
	return LOG_AND;
}

"||" {
	return LOG_OR;
}


{INTEGER} {
	return CONSTANT;
}

{BOOL_TRUE} {
	return CONSTANT;
}

{BOOL_FALSE} {
	return CONSTANT;
}

{IDENTIFIER} {
	return IDENTIFIER;
}

%%
