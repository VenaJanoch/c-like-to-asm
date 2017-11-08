%{

#include <stdio.h>
#include <stdlib.h>

#include "Compiler.h"

void yyerror(const char* s);

extern int yylex();
extern int yyparse();
extern FILE* yyin;
extern int yylineno;
extern char* yytext;

Compiler compiler;

%}

%locations
%define parse.error verbose

%token CONST STATIC VOID BOOL UINT8 UINT16 UINT32 STRING CONSTANT IDENTIFIER
%token IF ELSE RETURN DO WHILE FOR GOTO
%token INC_OP DEC_OP U_PLUS U_MINUS  
%token EQUAL NOT_EQUAL GREATER_OR_EQUAL LESS_OR_EQUAL SHIFT_LEFT SHIFT_RIGHT LOG_AND LOG_OR

%right '='
%left LOG_OR    
%left LOG_AND
%left '<' '>' LESS_OR_EQUAL GREATER_OR_EQUAL
%left EQUAL NOT_EQUAL
%left SHIFT_LEFT SHIFT_RIGHT
%left '+' '-'
%left '*' '/' '%'
%right U_PLUS U_MINUS '!'
%left INC_OP DEC_OP

%union
{
    char* string;
    int32_t integer;
    SymbolType type;
	ReturnSymbolType return_type;

	struct {
		char* value;
		SymbolType type;
		ExpressionType expression_type;

		BackpatchList* true_list;
		BackpatchList* false_list;
	} expression;

	struct {
		BackpatchList* next_list;
	} statement;

	struct {
	    SymbolTableEntry* list;
		int32_t count;
	} call_parameters;

	struct {
		int32_t ip;
		BackpatchList* next_list;
	} marker;
}

%type <string> id IDENTIFIER
%type <type> declaration_type declaration static_declaration
%type <return_type> return_type
%type <expression> expression assignment_with_declaration assignment CONSTANT
%type <statement> statement statement_list matched_statement unmatched_statement program function_body function
%type <call_parameters> call_parameter_list
%type <marker> marker jump_marker

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
			c.AddFunctionPrototype($2, $1);

			// Nothing to backpatch here...
            $$.next_list = nullptr;
        }
    | return_type id '(' parameter_list ')' function_body
        {
			c.AddFunction($2, $1);
            $$.next_list = $6.next_list;
        }
	| static_declaration_list
		{
			// Nothing to backpatch here...
			$$.next_list = nullptr;
		}
    ;

function_body
    : '{' statement_list  '}'
        {
            Log("P: Found function body");

			$$.next_list = $2.next_list;
        }
    ;

parameter_list
    : declaration_type id
        {
			c.ToParameterList($1, $2);

            Log("P: Found parameter");
        }
    | parameter_list ',' declaration_type id
        {
			c.ToParameterList($3, $4);

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

	// Statements
statement_list
    : statement
        {
			Log("P: Processing single statement in list statement list");

			$$.next_list = $1.next_list;
        }
    | statement_list marker statement
        {
			Log("P: Processing statement list");

			c.BackpatchStream($1.next_list, $2.ip);
			$$.next_list = $3.next_list;
        }
    ;

statement
    : matched_statement
        {
			Log("P: Processing matched statement");

			$$.next_list = $1.next_list;
        }
    | unmatched_statement
        {
			Log("P: Processing unmatched statement");

			$$.next_list = $1.next_list;
        }
	| declaration_list
		{
			Log("P: Processing declaration list");

			// Nothing to backpatch here...
			$$.next_list = nullptr;
		}
	| goto_label
		{
			Log("P: Processing goto label");

			// Nothing to backpatch here...
			$$.next_list = nullptr;
		}
    ;

matched_statement
    : IF '(' assignment ')' marker matched_statement jump_marker ELSE marker matched_statement
        {
			Log("P: Processing matched if..else");

			CheckIsIntOrBool($3, "Only integer and bool types are allowed in \"if\" statement", @3);
        }
    | assignment_with_declaration ';'
        {
			Log("P: Processing matched assignment");
		}
    | RETURN ';'
        {
			Log("P: Processing void return");
        }
    | RETURN assignment ';'
        {
			Log("P: Processing value return");
        }
    | WHILE marker '(' assignment ')' marker matched_statement jump_marker
        {
			Log("P: Processing matched while");

			CheckIsIntOrBool($4, "Only integer and bool types are allowed in \"while\" statement", @4);
        }
    | DO marker statement WHILE '(' marker assignment ')' ';'
        {
			Log("P: Processing matched do..while");

			CheckIsIntOrBool($7, "Only integer and bool types are allowed in \"while\" statement", @7);
        }
    | FOR '(' assignment_with_declaration ';' marker assignment ';' marker assignment jump_marker ')' marker matched_statement jump_marker
        {
            Log("P: Processing matched for");

            CheckIsInt($3, "Integer assignment is required in the first part of \"for\" statement", @3);
			CheckIsBool($6, "Bool expression is required in the middle part of \"for\" statement", @6);
			CheckIsInt($9, "Integer assignment is required in the last part of \"for\" statement", @9);
        }
	| GOTO id ';'
		{
			Log("P: Processing goto command");
		}
    | '{' statement_list '}'
        {
			Log("P: Processing statement block");
        }
    | '{' '}'
        {	    
			Log("P: Processing empty block");
        }
    ;

unmatched_statement
    : IF '(' assignment ')' marker statement
        {
			Log("P: Processing unmatched if");

			CheckIsIntOrBool($3, "Only integer and bool types are allowed in \"if\" statement", @3);
        }
    | WHILE marker '(' assignment ')' marker unmatched_statement jump_marker
        {
			Log("P: Processing unmatched while");

			CheckIsIntOrBool($4, "Only integer and bool types are allowed in \"while\" statement", @4);
        }
    | FOR '(' assignment_with_declaration ';' marker assignment ';' marker assignment jump_marker ')' marker unmatched_statement jump_marker
        {
            Log("P: Processing unmatched for");

			CheckIsInt($3, "Integer assignment is required in the first part of \"for\" statement", @3);
			CheckIsBool($6, "Bool expression is required in the middle part of \"for\" statement", @6);
			CheckIsInt($9, "Integer assignment is required in the last part of \"for\" statement", @9);
        }

    | IF '(' assignment ')' marker matched_statement jump_marker ELSE marker unmatched_statement
        {
			Log("P: Processing unmatched if else");

			CheckIsIntOrBool($3, "Only integer and bool types are allowed in \"if\" statement", @3);
        }
    ;
	
// Variable declaration, without assignment
declaration_list
    : declaration ';'
        {
            Log("P: Found declaration");
        }
    ;

declaration
    : declaration_type id
        {
            $$ = $1;
            c.ToDeclarationList($1, $2, ExpressionType::Variable);

            Log("P: Found variable declaration");
        }
    | declaration ',' id
        {
			CheckTypeIsValid($1, @1);
            c.ToDeclarationList($1, $3, ExpressionType::Variable);

            Log("P: Found multiple declarations");
        }
    ;

// Static variable declaration, without assignment
static_declaration_list
    : static_declaration ';'
        {
            Log("P: Found static declaration");
        }
    ;

static_declaration
    : STATIC declaration_type id
        {
            $$ = $2;
			c.AddStaticVariable($2, $3);

            Log("P: Found static variable declaration");
        }
    | STATIC static_declaration ',' id
        {
			CheckTypeIsValid($2, @2);
			c.AddStaticVariable($2, $4);

            Log("P: Found multiple static declarations");
        }
    ;

// Variable assignment, without type declaration
assignment
    : expression
        {
            Log("P: Found expression as assignment " << $1.value);

            $$ = $1;
        }
    | id '=' expression
		{
			Log("P: Found assignment");

            SymbolTableEntry* decl = c.GetParameter($1);
        	if (!decl) {
				std::string message = "Variable \"";
				message += $1;
				message += "\" is not declared in scope";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @1.first_line, @1.first_column);
        	}

			if (decl->expression_type == ExpressionType::Constant) {
				std::string message = "Variable \"";
				message += $1;
				message += "\" is read-only";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @3.first_line, @3.first_column);
        	}

			if (!c.CanImplicitCast(decl->type, $3.type, $3.expression_type)) {
				std::string message = "Cannot assign to variable \"";
				message += $1;
				message += "\" because of type mismatch";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @3.first_line, @3.first_column);
            }

            sprintf_s(output_buffer, "%s = %s", $1, $3.value);
            InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
			i->assignment.type = AssignType::None;
			i->assignment.dst_value = decl->name;
			i->assignment.op1_value = $3.value;
			i->assignment.op1_type = $3.type;
			i->assignment.op1_exp_type = $3.expression_type;

            $$.type = decl->type;
            $$.true_list = $3.true_list;
            $$.expression_type = ExpressionType::Variable;
            $$.value = $1;
        }
    ;

// Variable assignment, could be with or without type declaration
assignment_with_declaration
    : assignment
        {
            Log("P: Found assignment without declaration \"" << $1.value << "\"");

            $$ = $1;
        }
	| CONST declaration_type id '=' expression
		{
			Log("P: Found const. variable declaration with assignment \"" << $3 << "\"");

			if ($5.expression_type != ExpressionType::Constant) {
				std::string message = "Cannot assign non-constant value to variable \"";
				message += $3;
				message += "\"";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @5.first_line, @5.first_column);
			}

			if (!c.CanImplicitCast($2, $5.type, $5.expression_type)) {
				std::string message = "Cannot assign to variable \"";
				message += $3;
				message += "\" because of type mismatch";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @5.first_line, @5.first_column);
            }

            SymbolTableEntry* decl = c.ToDeclarationList($2, $3, ExpressionType::Constant);

            sprintf_s(output_buffer, "%s = %s", $3, $5.value);
            InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
			i->assignment.type = AssignType::None;
			i->assignment.dst_value = decl->name;
			i->assignment.op1_value = $5.value;
			i->assignment.op1_type = $5.type;
			i->assignment.op1_exp_type = $5.expression_type;

            $$.type = $2;
            $$.true_list = $5.true_list;
            $$.expression_type = ExpressionType::Constant;
            $$.value = $3;
        }
	| declaration_type id '=' expression
		{
			Log("P: Found variable declaration with assignment \"" << $2 << "\"");

			if (!c.CanImplicitCast($1, $4.type, $4.expression_type)) {
				std::string message = "Cannot assign to variable \"";
				message += $2;
				message += "\" because of type mismatch";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @4.first_line, @4.first_column);
            }

            SymbolTableEntry* decl = c.ToDeclarationList($1, $2, ExpressionType::None);

            sprintf_s(output_buffer, "%s = %s", $2, $4.value);
            InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
			i->assignment.type = AssignType::None;
			i->assignment.dst_value = decl->name;
			i->assignment.op1_value = $4.value;
			i->assignment.op1_type = $4.type;
			i->assignment.op1_exp_type = $4.expression_type;

            $$.type = $1;
            $$.true_list = $4.true_list;
            $$.expression_type = ExpressionType::Variable;
            $$.value = $2;
        }
    ;
expression
    : INC_OP expression
        {
			Log("P: Processing increment");

			CheckIsInt($2, "Specified type is not allowed in arithmetic operations", @1);
        }
    | DEC_OP expression
        {
			Log("P: Processing decrement");

			CheckIsInt($2, "Specified type is not allowed in arithmetic operations", @1);
        }
    | expression LOG_OR marker expression
        {

	    }
    | expression LOG_AND marker expression
		{

		}
    | expression NOT_EQUAL expression
        {
			Log("P: Processing logical not equal");

			CheckIsIntOrBool($1, "Only integer and bool types are allowed in comparsions", @1);
			CheckIsIntOrBool($3, "Only integer and bool types are allowed in comparsions", @3);
        }
    | expression EQUAL expression
        {
			Log("P: Processing logical equal");

			CheckIsIntOrBool($1, "Only integer and bool types are allowed in comparsions", @1);
			CheckIsIntOrBool($3, "Only integer and bool types are allowed in comparsions", @3);
        }
    | expression GREATER_OR_EQUAL expression
        {
			Log("P: Processing logical greater or equal");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);
        }
    | expression LESS_OR_EQUAL expression
        {
			Log("P: Processing logical smaller or equal");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);
        }
    | expression '>' expression
        {
			Log("P: Processing logical bigger");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);
        }
    | expression '<' expression
        {
			Log("P: Processing logical smaller");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);
        }
    | expression SHIFT_LEFT expression
        {
			Log("P: Processing left shift");

			CheckIsInt($1, "Only integer types are allowed in shift operations", @1);
			CheckIsInt($3, "Only integer types are allowed in shift operations", @3);
        }
	| expression SHIFT_RIGHT expression
        {
			Log("P: Processing left shift");

			CheckIsInt($1, "Only integer types are allowed in shift operations", @1);
			CheckIsInt($3, "Only integer types are allowed in shift operations", @3);
        }
    | expression '+' expression
        {
			Log("P: Processing addition");
        }
    | expression '-' expression
        {
			Log("P: Processing substraction");
        }
    | expression '*' expression
        {
			Log("P: Processing multiplication");
        }
    | expression '/' expression
        {
			Log("P: Processing division");
        }
    | expression '%' expression
        {
			Log("P: Processing remainder");
        }
    | '!' expression
        {
			Log("P: Processing logical not");
		}
    | U_PLUS expression
        {
			CheckIsInt($2, "Unary operator is not allowed in this context", @1);
        }
    | U_MINUS expression
        {
			CheckIsInt($2, "Unary operator is not allowed in this context", @1);
       }
    | CONSTANT
        {
			Log("P: Processing constant");
        }
    | '(' expression ')'
        {
            Log("P: Processing expression in parentheses");

			$$ = $2;
        }
    | id '(' call_parameter_list ')'
        {
			Log("P: Processing function call with parameters");
        }
    | id '('  ')'
        {
			Log("P: Processing function call");
        }
    | id
        {
			Log("P: Processing identifier");

			$$.value = $1;
			$$.type = param->type;
			$$.expression_type = ExpressionType::Variable;
        }
    ;

call_parameter_list
    : expression
        {
			Log("P: Processing call parameter list");

			CheckTypeIsValid($1.type, @1);

			$$.list = c.ToCallParameterList(nullptr, $1.type, $1.value, $1.expression_type);
			$$.count = 1;
        }
    | call_parameter_list ',' expression
        {
			Log("P: Processing call parameter list");

			CheckTypeIsValid($3.type, @3);

			$$.list = c.ToCallParameterList($1.list, $3.type, $3.value, $3.expression_type);
			$$.count = $1.count + 1;
        }
    ;

// Goto
// ToDo: Add beginning-of-line only marker?
goto_label
	: id ':'
		{
			Log("P: Found goto label \"" << $1 << "\"");

			c.AddLabel($1, $3.ip);
		}
	;

// Misc.
id
    : IDENTIFIER
        {
			Log("P: Found identifier \"" << $1 << "\"");

			$$ = _strdup(yytext);
        }
    ;

marker
	:	{	
			Log("P: Generating marker");

			$$.ip = c.NextIp();
		}
	;

jump_marker
	:	{
			Log("P: Generating jump marker");

			$$.ip = c.NextIp();
			sprintf_s(output_buffer, "goto");
			$$.next_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);
		}
	;

%%

int __cdecl wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	return compiler.OnRun(argc, argv);
}

void yyerror(const char* s)
{
	if (memcmp(s, "syntax error", 12) == 0) {
		if (memcmp(s + 12, ", ", 2) == 0) {
			throw CompilerException(CompilerExceptionSource::Syntax,
				s + 14, yylloc.first_line, yylloc.first_column);
		}

		throw CompilerException(CompilerExceptionSource::Syntax,
			s + 12, yylloc.first_line, yylloc.first_column);
	}

	throw CompilerException(CompilerExceptionSource::Syntax,
		s, yylloc.first_line, yylloc.first_column);
}

