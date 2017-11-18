%option noyywrap
%option yylineno
%option case-insensitive
%option nostdinit

%{
#include <stdio.h>
#include <stdbool.h>

#include "Log.h"
#include "Compiler.h"
#include "parser.tab.h"

#define YY_DECL int yylex()
#define YY_USER_ACTION                                  \
    yylloc.first_line = yylloc.last_line = yylineno;    \
    yylloc.first_column = yycolumn;                     \
    yylloc.last_column = yycolumn + yyleng - 1;         \
    yycolumn += yyleng;

int yycolumn = 1;
bool allow_unary = false;

extern Compiler c;

%}

NEWLINE (\n|\r\n)
WHITESPACE (" "|\t)

LINE_COMMENT ("//".*{NEWLINE})
BLOCK_COMMENT (\/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+\/)

DIRECTIVE ("#".*{NEWLINE})

INTEGER (0|[1-9][0-9]*)
BOOL_TRUE true
BOOL_FALSE false
STRING (\"[^\"]*\")
IDENTIFIER ([a-zA-Z_][a-zA-Z0-9_]*)

%%

<<EOF>> {
    FILE* yyin_old = yyin;

    yypop_buffer_state();

    if (yyin_old != yyin && yyin_old != stdin) {
        fclose(yyin_old);
    }

    if (!YY_CURRENT_BUFFER) {
        yyterminate();
    }
}

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

{DIRECTIVE} {
    c.ParseCompilerDirective(yytext, [&](char* directive, char* param) {
        LogDebug("L: Found preprocessor directive \"" << directive << "\"");

        if (param && strcmp(directive, "#include") == 0) {
            // Include file directive
            char* path_start = param;
            if (*path_start == '"') {
                path_start++;
            }

            char* path_end = path_start;
            while (*path_end && *path_end != '"') {
                path_end++;
            }

            char* path = new char[path_end - path_start + 1];
            memcpy(path, path_start, path_end - path_start);
            path[path_end - path_start] = '\0';

            errno_t err = fopen_s(&yyin, path, "rb");

            delete path;

            if (err) {
                throw CompilerException(CompilerExceptionSource::Unknown, "Cannot open include file");
            }

            yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));

            BEGIN(INITIAL);
            return true;
        }
    
        return false;
    });
}

^"-" {
    LogDebug("L: Found unary minus");

    allow_unary = false;
    return U_MINUS;
}

^"+" {
    LogDebug("L: Found unary plus");

    allow_unary = false;
    return U_PLUS;
}

("("|"{"|"<"|">"|"="|";"|","|"!"|":") {
    LogDebug("L: Found " << yytext[0]);

    allow_unary = true;
    return yytext[0];
}

(")"|"}") {
    LogDebug("L: Found " << yytext[0]);

    allow_unary = false;
    return yytext[0];
}

("/"|"*"|"%") {
    LogDebug("L: Found " << yytext[0]);

    allow_unary = true;
    return yytext[0];
}

"-" {
    if (allow_unary) {
        LogDebug("L: Found unary minus");

        allow_unary = false;
        return U_MINUS;
    } else {
        LogDebug("L: Found minus");

        return yytext[0];
    }
}

"+" {
    if (allow_unary) {
        LogDebug("L: Found unary plus");

        allow_unary = false;
        return U_PLUS;
    } else {
        LogDebug("L: Found plus");

        return yytext[0];
    }
}

"const" {
    LogDebug("L: Found \"const\"");

    return CONST;
}

"static" {
    LogDebug("L: Found \"static\"");

    return STATIC;
}

"void" {
    LogDebug("L: Found \"void\"");

    return VOID;
}

"bool" {
    LogDebug("L: Found \"bool\"");

    return BOOL;
}

"uint8" {
    LogDebug("L: Found \"uint8\"");

    return UINT8;
}

"uint16" {
    LogDebug("L: Found \"uint16\"");

    return UINT16;
}

"uint32" {
    LogDebug("L: Found \"uint32\"");

    return UINT32;
}

"string" {
    LogDebug("L: Found \"string\"");

    return STRING;
}

"if" {
    LogDebug("L: Found \"if\"");

    return IF;
}

"else" {
    LogDebug("L: Found \"else\"");

    return ELSE;
}

"return" {
    LogDebug("L: Found \"return\"");

    return RETURN;
}

"do" {
    LogDebug("L: Found \"do\"");

    return DO;
}

"while" {
    LogDebug("L: Found \"while\"");

    return WHILE;
}

"for" {
    LogDebug("L: Found \"for\"");

    return FOR;
}

"switch" {
    LogDebug("L: Found \"switch\"");

    return SWITCH;
}

"case" {
    LogDebug("L: Found \"case\"");

    return CASE;
}

"default" {
    LogDebug("L: Found \"default\"");

    return DEFAULT;
}

"continue" {
    LogDebug("L: Found \"continue\"");

    return CONTINUE;
}

"break" {
    LogDebug("L: Found \"break\"");

    return BREAK;
}

"goto" {
    LogDebug("L: Found \"goto\"");

    return GOTO;
}

"++" {
    LogDebug("L: Found ++");

    return INC_OP;
}

"--" {
    LogDebug("L: Found --");

    return DEC_OP;
}

"==" {
    LogDebug("L: Found ==");

    return EQUAL;
}

"!=" {
    LogDebug("L: Found !=");

    return NOT_EQUAL;
}

">=" {
    LogDebug("L: Found >=");

    return GREATER_OR_EQUAL;
}

"<=" {
    LogDebug("L: Found <=");

    return LESS_OR_EQUAL;
}

"<<" {
    LogDebug("L: Found <<");

    return SHIFT_LEFT;
}

">>" {
    LogDebug("L: Found >>");

    return SHIFT_RIGHT;
}

"&&" {
    LogDebug("L: Found &&");

    return LOG_AND;
}

"||" {
    LogDebug("L: Found ||");

    return LOG_OR;
}

{INTEGER} {
    LogDebug("L: Found integer constant \"" << yytext << "\"");

    yylval.expression.value = _strdup(yytext);
    yylval.expression.exp_type = ExpressionType::Constant;

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
    LogDebug("L: Found bool constant \"true\"");

    yylval.expression.value = _strdup("1");
    yylval.expression.exp_type = ExpressionType::Constant;
    yylval.expression.type = SymbolType::Bool;
    allow_unary = false;
    return CONSTANT;
}

{BOOL_FALSE} {
    LogDebug("L: Found bool constant \"false\"");

    yylval.expression.value = _strdup("0");
    yylval.expression.exp_type = ExpressionType::Constant;
    yylval.expression.type = SymbolType::Bool;
    allow_unary = false;
    return CONSTANT;
}

{STRING} {
    LogDebug("L: Found string constant \"" << yytext << "\"");

    char* value = new char[yyleng - 1];
    memcpy(value, yytext + 1, yyleng - 2);
    value[yyleng - 2] = '\0';

    yylval.expression.value = value;
    yylval.expression.exp_type = ExpressionType::Constant;
    yylval.expression.type = SymbolType::String;
    allow_unary = false;
    return CONSTANT;
}

{IDENTIFIER} {
    LogDebug("L: Found identifier \"" << yytext << "\"");

    yylval.string = _strdup(yytext);
    allow_unary = false;
    return IDENTIFIER;
}

%%
