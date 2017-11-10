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

			c.BackpatchStream($3.true_list, $5.ip);
			c.BackpatchStream($3.false_list, $9.ip);
			$$.next_list = MergeLists($7.next_list, $10.next_list);
			$$.next_list = MergeLists($$.next_list, $6.next_list);
        }
    | assignment_with_declaration ';'
        {
			Log("P: Processing matched assignment");

			// Nothing to backpatch here...
			$$.next_list = nullptr;
		}
    | RETURN ';'
        {
			Log("P: Processing void return");

			$$.next_list = nullptr;
			sprintf_s(output_buffer, "return");
			InstructionEntry* i = c.AddToStream(InstructionType::Return, output_buffer);
			i->return_statement.type = ExpressionType::None;
        }
    | RETURN assignment ';'
        {
			Log("P: Processing value return");

			$$.next_list = nullptr;
			sprintf_s(output_buffer, "return %s", $2.value);
			InstructionEntry* i = c.AddToStream(InstructionType::Return, output_buffer);
			i->return_statement.value = $2.value;
			i->return_statement.type = $2.expression_type;
        }
    | WHILE marker '(' assignment ')' marker matched_statement jump_marker
        {
			Log("P: Processing matched while");

			CheckIsIntOrBool($4, "Only integer and bool types are allowed in \"while\" statement", @4);

			c.BackpatchStream($4.true_list, $6.ip);
			$$.next_list = $4.false_list;
			c.BackpatchStream($7.next_list, $2.ip);
			c.BackpatchStream($8.next_list, $2.ip);
        }
    | DO marker statement WHILE '(' marker assignment ')' ';'
        {
			Log("P: Processing matched do..while");

			CheckIsIntOrBool($7, "Only integer and bool types are allowed in \"while\" statement", @7);

			c.BackpatchStream($3.next_list, $6.ip);
			c.BackpatchStream($7.true_list, $2.ip);
			$$.next_list = $7.false_list;
        }
    | FOR '(' assignment_with_declaration ';' marker assignment ';' marker assignment jump_marker ')' marker matched_statement jump_marker
        {
            Log("P: Processing matched for");

            CheckIsInt($3, "Integer assignment is required in the first part of \"for\" statement", @3);
			CheckIsBool($6, "Bool expression is required in the middle part of \"for\" statement", @6);
			CheckIsInt($9, "Integer assignment is required in the last part of \"for\" statement", @9);

            c.BackpatchStream($3.true_list, $5.ip);
            c.BackpatchStream($13.next_list, $8.ip);
            c.BackpatchStream($14.next_list, $8.ip);
            $$.next_list = $6.false_list;
            c.BackpatchStream($6.true_list, $12.ip);
            c.BackpatchStream($9.true_list, $5.ip);
            c.BackpatchStream($10.next_list, $5.ip);
        }
	| GOTO id ';'
		{
			Log("P: Processing goto command");

			$$.next_list = nullptr;
			sprintf_s(output_buffer, "goto %s", $2);
			InstructionEntry* i = c.AddToStream(InstructionType::GotoLabel, output_buffer);
			i->goto_label_statement.label = $2;
		}
    | '{' statement_list '}'
        {
			Log("P: Processing statement block");

			$$.next_list = $2.next_list;
        }
    | '{' '}'
        {	    
			Log("P: Processing empty block");

			$$.next_list = NULL;
        }
    ;

unmatched_statement
    : IF '(' assignment ')' marker statement
        {
			Log("P: Processing unmatched if");

			CheckIsIntOrBool($3, "Only integer and bool types are allowed in \"if\" statement", @3);

			c.BackpatchStream($3.true_list, $5.ip);
			$$.next_list = MergeLists($3.false_list, $6.next_list);
        }
    | WHILE marker '(' assignment ')' marker unmatched_statement jump_marker
        {
			Log("P: Processing unmatched while");

			CheckIsIntOrBool($4, "Only integer and bool types are allowed in \"while\" statement", @4);

			c.BackpatchStream($4.true_list, $6.ip);
			$$.next_list = $4.false_list;
			c.BackpatchStream($7.next_list, $2.ip);
			c.BackpatchStream($8.next_list, $2.ip);
        }
    | FOR '(' assignment_with_declaration ';' marker assignment ';' marker assignment jump_marker ')' marker unmatched_statement jump_marker
        {
            Log("P: Processing unmatched for");

			CheckIsInt($3, "Integer assignment is required in the first part of \"for\" statement", @3);
			CheckIsBool($6, "Bool expression is required in the middle part of \"for\" statement", @6);
			CheckIsInt($9, "Integer assignment is required in the last part of \"for\" statement", @9);

            c.BackpatchStream($3.true_list, $5.ip);
            c.BackpatchStream($13.next_list, $8.ip);
            c.BackpatchStream($14.next_list, $8.ip);
            $$.next_list = $6.false_list;
            c.BackpatchStream($6.true_list, $12.ip);
            c.BackpatchStream($9.true_list, $5.ip);
            c.BackpatchStream($10.next_list, $5.ip);
        }

    | IF '(' assignment ')' marker matched_statement jump_marker ELSE marker unmatched_statement
        {
			Log("P: Processing unmatched if else");

			CheckIsIntOrBool($3, "Only integer and bool types are allowed in \"if\" statement", @3);

			c.BackpatchStream($3.true_list, $5.ip);
			c.BackpatchStream($3.false_list, $9.ip);
			$$.next_list = MergeLists($7.next_list, $10.next_list);
			$$.next_list = MergeLists($$.next_list, $6.next_list);
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

			SymbolTableEntry* decl;

			// Create a variable if needed
			if ($2.expression_type != ExpressionType::Variable) {
				decl = c.GetUnusedVariable($2.type);

				sprintf_s(output_buffer, "%s = %s", decl->name, $2.value);
				InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
				i->assignment.dst_value = decl->name;
				i->assignment.op1_value = $2.value;
				i->assignment.op1_type = $2.type;
				i->assignment.op1_exp_type = $2.expression_type;

				$2.value = decl->name;
				$2.type = decl->type;
				$2.expression_type = ExpressionType::Variable;
			} else {
				decl = c.GetParameter($2.value);
			}

            sprintf_s(output_buffer, "%s = %s + 1", $2.value, $2.value);
            InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
			i->assignment.type = AssignType::Add;
			i->assignment.dst_value = decl->name;
			i->assignment.op1_value = $2.value;
			i->assignment.op1_type = $2.type;
			i->assignment.op1_exp_type = $2.expression_type;
			i->assignment.op2_value = _strdup("1");
			i->assignment.op2_type = $2.type;
			i->assignment.op2_exp_type = ExpressionType::Constant;

            $$ = $2;
            $$.true_list = nullptr;
            $$.false_list = nullptr;
        }
    | DEC_OP expression
        {
			Log("P: Processing decrement");

			CheckIsInt($2, "Specified type is not allowed in arithmetic operations", @1);

			SymbolTableEntry* decl;

			// Create a variable if needed
			if ($2.expression_type != ExpressionType::Variable) {
				decl = c.GetUnusedVariable($2.type);

				sprintf_s(output_buffer, "%s = %s", decl->name, $2.value);
				InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
				i->assignment.dst_value = decl->name;
				i->assignment.op1_value = $2.value;
				i->assignment.op1_type = $2.type;
				i->assignment.op1_exp_type = $2.expression_type;

				$2.value = decl->name;
				$2.type = decl->type;
				$2.expression_type = ExpressionType::Variable;
			} else {
				decl = c.GetParameter($2.value);
			}

            sprintf_s(output_buffer, "%s = %s - 1", $2.value, $2.value);
            InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
			i->assignment.type = AssignType::Subtract;
			i->assignment.dst_value = decl->name;
			i->assignment.op1_value = $2.value;
			i->assignment.op1_type = $2.type;
			i->assignment.op1_exp_type = $2.expression_type;
			i->assignment.op2_value = _strdup("1");
			i->assignment.op2_type = $2.type;
			i->assignment.op2_exp_type = ExpressionType::Constant;

            $$ = $2;
            $$.true_list = nullptr;
            $$.false_list = nullptr;
        }
    | expression LOG_OR marker expression
        {
			if (SymbolType::Bool != $1.type) {
                sprintf_s(output_buffer, "if (%s != 0) goto", $1.value);
                CreateIfConstWithBackpatch($1.true_list, CompareType::NotEqual, $1, "0");
                
				sprintf_s(output_buffer, "goto");
                $1.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);
            }

            if (SymbolType::Bool != $4.type) {
                sprintf_s(output_buffer, "if (%s != 0) goto", $4.value);
                CreateIfConstWithBackpatch($4.true_list, CompareType::NotEqual, $4, "0");
                
				sprintf_s(output_buffer, "goto");
                $4.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);
            }

            $$.true_list = MergeLists($1.true_list, $4.true_list);
            c.BackpatchStream($1.false_list, $3.ip);
            $$.false_list = $4.false_list;
            $$.type = SymbolType::Bool;
	    }
    | expression LOG_AND marker expression
		{
			if (SymbolType::Bool != $1.type) {
				sprintf_s(output_buffer, "if (%s != 0) goto", $1.value);
				CreateIfConstWithBackpatch($1.true_list, CompareType::NotEqual, $1, "0");

				sprintf_s(output_buffer, "goto");
				$1.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);
			}

			if (SymbolType::Bool != $4.type) {
				sprintf_s(output_buffer, "if (%s != 0) goto", $4.value);
				CreateIfConstWithBackpatch($4.true_list, CompareType::NotEqual, $4, "0");

				sprintf_s(output_buffer, "goto");
				$4.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);
			}

			$$.false_list = MergeLists($1.false_list, $4.false_list);
			c.BackpatchStream($1.true_list, $3.ip);
			$$.true_list = $4.true_list;
			$$.type = SymbolType::Bool;
		}
    | expression NOT_EQUAL expression
        {
			Log("P: Processing logical not equal");

			CheckIsIntOrBool($1, "Only integer and bool types are allowed in comparsions", @1);
			CheckIsIntOrBool($3, "Only integer and bool types are allowed in comparsions", @3);

			sprintf_s(output_buffer, "if (%s != %s) goto", $1.value, $3.value);
			CreateIfWithBackpatch($$.true_list, CompareType::NotEqual, $1, $3);

			sprintf_s(output_buffer, "goto");
			$$.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);

			$$.type = SymbolType::Bool;
			$$.expression_type = ExpressionType::None;
        }
    | expression EQUAL expression
        {
			Log("P: Processing logical equal");

			CheckIsIntOrBool($1, "Only integer and bool types are allowed in comparsions", @1);
			CheckIsIntOrBool($3, "Only integer and bool types are allowed in comparsions", @3);

			sprintf_s(output_buffer, "if (%s == %s) goto", $1.value, $3.value);
			CreateIfWithBackpatch($$.true_list, CompareType::Equal, $1, $3);

			sprintf_s(output_buffer, "goto");
			$$.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);

			if ($1.type == SymbolType::Bool) {
				$$.true_list = MergeLists($$.true_list, $1.true_list);
				$$.false_list = MergeLists($$.false_list, $1.false_list);
			}
			if ($3.type == SymbolType::Bool) {
				$$.true_list = MergeLists($$.true_list, $3.true_list);
				$$.false_list = MergeLists($$.false_list, $3.false_list);
			}

			$$.type = SymbolType::Bool;
			$$.expression_type = ExpressionType::None;
        }
    | expression GREATER_OR_EQUAL expression
        {
			Log("P: Processing logical greater or equal");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);

			sprintf_s(output_buffer, "if (%s >= %s) goto", $1.value, $3.value);
			CreateIfWithBackpatch($$.true_list, CompareType::GreaterOrEqual, $1, $3);

			sprintf_s(output_buffer, "goto");
			$$.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);

			$$.value = "TrueFalse Only!";
			$$.type = SymbolType::Bool;
			$$.expression_type = ExpressionType::None;
        }
    | expression LESS_OR_EQUAL expression
        {
			Log("P: Processing logical smaller or equal");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);

			sprintf_s(output_buffer, "if (%s <= %s) goto", $1.value, $3.value);
			CreateIfWithBackpatch($$.true_list, CompareType::LessOrEqual, $1, $3);

			sprintf_s(output_buffer, "goto");
			$$.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);

			$$.value = "TrueFalse Only!";
			$$.type = SymbolType::Bool;
			$$.expression_type = ExpressionType::None;
			printf("P: Done processing logical smaller or equal.\n");
        }
    | expression '>' expression
        {
			Log("P: Processing logical bigger");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);

			sprintf_s(output_buffer, "if (%s > %s) goto", $1.value, $3.value);
			CreateIfWithBackpatch($$.true_list, CompareType::Greater, $1, $3);

			sprintf_s(output_buffer, "goto");
			$$.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);

			$$.value = "TrueFalse Only!";
			$$.type = SymbolType::Bool;
			$$.expression_type = ExpressionType::None;
        }
    | expression '<' expression
        {
			Log("P: Processing logical smaller");

			CheckIsInt($1, "Only integer types are allowed in comparsions", @1);
			CheckIsInt($3, "Only integer types are allowed in comparsions", @3);

			sprintf_s(output_buffer, "if (%s < %s) goto", $1.value, $3.value);
			CreateIfWithBackpatch($$.true_list, CompareType::Less, $1, $3);

			sprintf_s(output_buffer, "goto");
			$$.false_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);

			$$.value = "TrueFalse Only!";
			$$.type = SymbolType::Bool;
			$$.expression_type = ExpressionType::None;
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

			if ($2.type == SymbolType::Bool) {
				$$ = $2;
				$$.true_list = $2.false_list;
				$$.false_list = $2.true_list;
			} else if ($2.type == SymbolType::Uint8 || $2.type == SymbolType::Uint16 || $2.type == SymbolType::Uint32) {
				sprintf_s(output_buffer, "if (%s != 0) goto", $2.value);
				CreateIfConstWithBackpatch($$.false_list, CompareType::NotEqual, $2, "0");

				sprintf_s(output_buffer, "goto");
				$$.true_list = c.AddToStreamWithBackpatch(InstructionType::Goto, output_buffer);
			} else {
				throw CompilerException(CompilerExceptionSource::Statement,
					"Specified type is not allowed in logical operations", @1.first_line, @1.first_column);
			}
		}
    | U_PLUS expression
        {
			CheckIsInt($2, "Unary operator is not allowed in this context", @1);

			$$ = $2;
        }
    | U_MINUS expression
        {
			CheckIsInt($2, "Unary operator is not allowed in this context", @1);

			SymbolTableEntry* decl = c.GetUnusedVariable($2.type);
			decl->expression_type = $2.expression_type;

			sprintf_s(output_buffer, "%s = -%s", decl->name, $2.value);
			InstructionEntry* i = c.AddToStream(InstructionType::Assign, output_buffer);
			i->assignment.type = AssignType::Negation;
			i->assignment.dst_value = decl->name;
			i->assignment.op1_value = $2.value;
			i->assignment.op1_type = $2.type;
			i->assignment.op1_exp_type = $2.expression_type;

			$$ = $2;
			$$.value = decl->name;
			$$.expression_type = ExpressionType::Variable;
       }
    | CONSTANT
        {
			Log("P: Processing constant");

			$$.value = _strdup($1.value);
			$$.expression_type = ExpressionType::Constant;
            $$.true_list = nullptr;
			$$.false_list = nullptr;
        }
    | '(' expression ')'
        {
            Log("P: Processing expression in parentheses");

			$$ = $2;
        }
    | id '(' call_parameter_list ')'
        {
			Log("P: Processing function call with parameters");

            SymbolTableEntry* func = c.GetFunction($1);
            if (!func || func->return_type == ReturnSymbolType::Unknown) {
				std::string message = "Function \"";
				message += $1;
				message += "\" is not defined";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @1.first_line, @1.first_column);
            }

			if (func->return_type == ReturnSymbolType::Void) {
				// Has void return
				$$.type = SymbolType::None;
				$$.value = nullptr;
				$$.expression_type = ExpressionType::None;
				c.PrepareForCall($1, $3.list, $3.count);

				sprintf_s(output_buffer, "call %s (%d)", $1, $3.count);
				InstructionEntry* i = c.AddToStream(InstructionType::Call, output_buffer);
				i->call_statement.target = func;
			} else {
				// Has return value
				switch (func->return_type) {
					case ReturnSymbolType::Bool: $$.type = SymbolType::Bool; break;
					case ReturnSymbolType::Uint8: $$.type = SymbolType::Uint8; break;
					case ReturnSymbolType::Uint16: $$.type = SymbolType::Uint16; break;
					case ReturnSymbolType::Uint32: $$.type = SymbolType::Uint32; break;

					default: throw CompilerException(CompilerExceptionSource::Unknown, "Internal error");
				}

				SymbolTableEntry* decl = c.GetUnusedVariable($$.type);

				$$.value = decl->name;
				$$.expression_type = ExpressionType::Variable;
				c.PrepareForCall($1, $3.list, $3.count);

				sprintf_s(output_buffer, "%s = call %s (%d)", decl->name, $1, $3.count);
				InstructionEntry* i = c.AddToStream(InstructionType::Call, output_buffer);
				i->call_statement.target = func;
				i->call_statement.return_symbol = decl->name;
			}
        }
    | id '('  ')'
        {
			Log("P: Processing function call");

			SymbolTableEntry* func = c.GetFunction($1);
            if (!func || func->return_type == ReturnSymbolType::Unknown) {
				std::string message = "Function \"";
				message += $1;
				message += "\" is not defined";
				throw CompilerException(CompilerExceptionSource::Statement,
					message, @1.first_line, @1.first_column);
            }

			if (func->return_type == ReturnSymbolType::Void) {
				// Has return value
				$$.type = SymbolType::None;
				$$.value = nullptr;
				$$.expression_type = ExpressionType::None;
				c.PrepareForCall($1, nullptr, 0);

				sprintf_s(output_buffer, "call %s (%d)", $1, 0);
				InstructionEntry* i = c.AddToStream(InstructionType::Call, output_buffer);
				i->call_statement.target = func;
			} else {
				// Has void return
				switch (func->return_type) {
					case ReturnSymbolType::Bool: $$.type = SymbolType::Bool; break;
					case ReturnSymbolType::Uint8: $$.type = SymbolType::Uint8; break;
					case ReturnSymbolType::Uint16: $$.type = SymbolType::Uint16; break;
					case ReturnSymbolType::Uint32: $$.type = SymbolType::Uint32; break;

					default: throw CompilerException(CompilerExceptionSource::Unknown, "Internal error");
				}

				SymbolTableEntry* decl = c.GetUnusedVariable($$.type);

				$$.value = decl->name;
				$$.expression_type = ExpressionType::Variable;
				c.PrepareForCall($1, nullptr, 0);

				sprintf_s(output_buffer, "%s = call %s (%d)", decl->name, $1, 0);
				InstructionEntry* i = c.AddToStream(InstructionType::Call, output_buffer);
				i->call_statement.target = func;
				i->call_statement.return_symbol = decl->name;
			}
        }
    | id
        {
			Log("P: Processing identifier");

			SymbolTableEntry* param = c.GetParameter($1);
            if (!param) {
				std::string message = "Variable \"";
                message += $1;
                message += "\" is not declared in scope";
                throw CompilerException(CompilerExceptionSource::Statement,
					message, @1.first_line, @1.first_column);
            }

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

