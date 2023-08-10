#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "scanner.h"
#include "vm.h"
#include "value.h"
#include "object.h"
#include <stdlib.h>
#include <math.h>

typedef struct Local
{
	Token name;
	int depth;
}
Local;

typedef struct Compiler
{
	Local locals[UINT8_MAX];
	Table const_global;
	int local_count;
	int scope_depth;
}
Compiler;

Parser parser;
Chunk * compiling_chunk;
Compiler * current = NULL;

static void compiler_init(Compiler * compiler)
{
	compiler->scope_depth = compiler->local_count = 0;
	current = compiler;
}

static void compiler_free()
{
	current = NULL;
}

void parser_init()
{
	parser.error = false;
	parser.panic = false;
	parser.prev_prec = PREC_NONE;
}

static Chunk * current_chunk()
{
	return compiling_chunk;
}

static void emit_error_msg(Token * tk_error, const char * msg)
{
	if (parser.panic)
		return;
	parser.panic = true;

	fprintf(stderr, "On line %u", tk_error->line);
	if (tk_error->type == TK_EOF)
		fprintf(stderr, ", at end: %s\n", msg);
	else if (tk_error->type != TK_ERROR)
		fprintf(stderr, ", at token '%.*s': %s\n", tk_error->length,
				tk_error->start, msg); 
	else
		fprintf(stderr, ": %.*s\n", tk_error->length, tk_error->start);
}

static void error(Token * tk_error, const char * msg)
{
	parser.error = true;
	emit_error_msg(tk_error, msg);
}


static void emit_byte(uint8_t byte)
{
	chunk_append(current_chunk(), byte, parser.prev.line);
}

static void emit_bytes(void * bytes, uint8_t byte_count)
{
	chunk_append_bytes(current_chunk(), bytes, byte_count);
}

/** @emit_param_inst: write instruction that has one parameter 
 * (OP_CONST, OP_CONST_LONG...) */
static void emit_param_inst(Opcode opcode, uint32_t param, uint8_t param_sz)
{
	emit_byte(opcode);
	emit_bytes(&param, param_sz);
}

static uint32_t emit_jump(Opcode jmp_opcode)
{
	emit_byte(jmp_opcode);
	// parameter's starting position
	uint32_t jmp_param_pos = current_chunk()->size;
	// placeholder for parameter
	emit_byte(0xff);
	emit_byte(0xff);
	return jmp_param_pos;
}

static void emit_loop(uint32_t loop_start)
{
	emit_byte(OP_LOOP);
	uint32_t offset = current_chunk()->size + 2 - loop_start;
	if (offset > UINT16_MAX)
		error(&parser.prev, "Loop body is too large.");
	emit_bytes(&offset, 2);
}

/** @emit_const_inst: write a load instruction that load @val
 * to the virtual machine.
 */
static void emit_const_inst(Value val)
{
	if (chunk_get_const_pool_size(current_chunk()) + 1 > CONST_POOL_LIMIT)
	{
		error(&parser.current, "[Memory error] Too much constant in one chunk.");
	}

	chunk_write_load_const(current_chunk(), val, parser.prev.line);
}

static uint32_t identifier_constant(Token * tk)
{
	StringObj * identifier = StringObj_construct(tk->start, tk->length);
	return chunk_add_const(current_chunk(), OBJ_VAL(*identifier));
}

static void advance()
{
	parser.prev = parser.current;
	while (true)
	{
		parser.current = scan_token();
		if (parser.current.type != TK_ERROR)
			break;
		error(&parser.current, parser.current.start);
	}
}

static bool check(TokenType type)
{
	return parser.current.type == type;
}

static void synchronize()
{
	parser.panic = false;
	while (parser.current.type != TK_EOF)
	{
		if (parser.prev.type == TK_SEMICOLON)
			break;

		switch (parser.current.type)
		{
			case TK_VAR:
			case TK_PRINT:
			case TK_CLASS:
			case TK_FUN:
			case TK_WHILE:
			case TK_FOR:
			case TK_IF:
			case TK_RETURN:
				break;
			default:
		}

		advance();
	}

	parser.panic = false;
}

static bool match(TokenType type)
{
	if (parser.current.type == type)
	{
		advance();
		return true;
	}

	return false;
}

/* consume: consume the current token if its type is @type
 *
 * throw an error to stderr if the next token does not
 * belong to @type
 * */
static void consume(TokenType type, const char * msg)
{
	if (parser.current.type == type)
	{
		advance();
		return;
	}

	error(&parser.current, msg);
}


static void emit_return()
{
	emit_byte(OP_RETURN);
}

static void end_compiler()
{
	emit_byte(OP_EXIT);
}

static void unary();
static void binary();
static void grouping();
static void number();
static void literal();
static void string();
static void variable();
static void assignment();
static void stmt();
static void block_stmt();
static void var_declaration();
static void if_stmt();
static void while_stmt();
static void for_stmt();
static uint32_t parse_identifier(const char * error_msg);
static void declare_variable(Token name);
static void define_variable(uint32_t offset);
static void and_();
static void or_();

typedef void (*ParseFn) ();	// pointer to a parsing function

typedef struct ParseRule
{
	ParseFn prefix;		// function called if the current operation is prefix
	ParseFn infix;		// function called if the current operation is infix
	Precedence prec;	// precedence associated to the current infix operation
}
ParseRule;

/* parse_rules: a mapping from operators to appropiate parsing rules */
ParseRule parse_rules[] = {
  [TK_TRUE]	     = {literal, NULL, PREC_PRIMARY},
  [TK_FALSE]	     = {literal, NULL, PREC_PRIMARY},
  [TK_NIL]	     = {literal, NULL, PREC_PRIMARY},
  [TK_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TK_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TK_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TK_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TK_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TK_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TK_MINUS]         = {unary,    binary, PREC_TERM},
  [TK_PLUS]          = {NULL,     binary, PREC_TERM},
  [TK_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TK_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TK_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TK_BANG]          = {unary, 	  NULL,   PREC_UNARY},
  [TK_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
  [TK_EQUAL]         = {NULL,     assignment,   PREC_ASSIGNMENT},
  [TK_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
  [TK_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
  [TK_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
  [TK_LESS]          = {NULL,     binary,   PREC_COMPARISON},
  [TK_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},
  [TK_IDENTIFIER]    = {variable,     NULL,   PREC_NONE},
  [TK_STRING]        = {string,     NULL,   PREC_PRIMARY},
  [TK_NUMBER]        = {number,   NULL,   PREC_PRIMARY},
  [TK_AND]           = {NULL,     and_, PREC_AND},
  [TK_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TK_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TK_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TK_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TK_IF]            = {NULL,     NULL,   PREC_NONE},
  [TK_OR]            = {NULL,     or_, PREC_OR},
  [TK_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TK_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TK_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TK_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TK_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TK_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TK_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TK_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule * get_rule(TokenType op)
{
	return &(parse_rules[op]);
}

static bool identifier_equal(Token * tk_1, Token * tk_2) 
{
	return tk_1->length == tk_2->length &&
		memcmp(tk_1->start, tk_2->start, tk_1->length) == 0;
}


/* parse_precedence: parse the expression whose precedence is
 * not lower than @prec
 * @prec: precedence's lower bound
 * */
static void parse_precedence(Precedence prec)
{
	// a valid expression always start with a prefix expression
	// or a literal

	advance();	// consume the prefix operator or a literal
	ParseFn prefix_rule = get_rule(parser.prev.type)->prefix;

	if (prefix_rule == NULL)
	{
		error(&parser.prev, "Expect an expression.");
		return;
	}

	prefix_rule();	// call the appropriate function to parse the prefix expression or literal
	parser.prev_prec = get_rule(parser.prev.type)->prec;

	while (get_rule(parser.current.type)->prec >= prec && !parser.panic)
	{
		if (get_rule(parser.current.type)->prec == PREC_PRIMARY) 
		{
			error(&parser.current, "Expect an operator here.");
			break;
		}

		advance();	// consume the infix operator
		// find the appropriate function to parse the second operand
		ParseFn infix_rule = get_rule(parser.prev.type)->infix;
		infix_rule();
	}
}


static void expression()
{
	parser.prev_prec = PREC_NONE;
	parse_precedence(PREC_ASSIGNMENT);
}

static int resolve_variable(Compiler * compiler, Token * name)
{
	for (int i = compiler->local_count - 1; i >= 0; i--) {
		Local * current_local = &compiler->locals[i];
		if (identifier_equal(name, &current_local->name))
			return i;
	}
	return -1;
}

static void assignment()
{
	if (parser.prev_prec >= PREC_ASSIGNMENT)
	{
		error(&parser.prev, "Invalid assignment.");
		return;
	}

	// since the previous identifier is added recently, it lies on top
	// of the constant pool
	uint32_t iden_offset = current_chunk()->constants.size - 1;
	Token name = parser.consumed_identifier;	// used to resolve variable's name

	parse_precedence(PREC_ASSIGNMENT);
	uint32_t stack_index = resolve_variable(current, &name);

	if (stack_index == -1)
		emit_param_inst(OP_SET_GLOBAL, iden_offset, 3);
	else
		emit_param_inst(OP_SET_LOCAL, stack_index, 3);
}

static void variable()
{
	parser.consumed_identifier = parser.prev;
	uint32_t offset = identifier_constant(&parser.prev);
	
	// if the next operation is assignment, no need to load the variable
	// to the stack
	if (parser.current.type == TK_EQUAL)
		return;
	
	uint32_t stack_index = resolve_variable(current, &parser.prev);
	if (stack_index == -1)
		emit_param_inst(OP_GET_GLOBAL, offset, 3);
	else
		emit_param_inst(OP_GET_LOCAL, stack_index, 3);
}


static void declaration()
{
	if (match(TK_VAR))
		var_declaration();
	else
		stmt();

	if (parser.panic)
		synchronize();
}

static uint16_t patch_jump(uint32_t jmp_param_pos)
{
	// the position where the jump starts. We read the jump parameter before starting to jump.
	uint32_t jump_pos = jmp_param_pos + 2;
	uint32_t dest = current_chunk()->size;
	uint32_t jump_dist = dest - jump_pos;
	if (jump_dist >= UINT16_MAX) 
	{
		error(&parser.prev, "Too much bytecodes to jump.");
		return 0;
	}
	memcpy(&(current_chunk()->bytecodes[jmp_param_pos]), &jump_dist, 2);
	return (uint16_t) jump_dist;
}

static void if_stmt()
{
	consume(TK_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TK_RIGHT_PAREN, "Expect ')' after condition.");
	uint32_t jmp_then = emit_jump(OP_JMP_IF_FALSE);
	stmt();
	if (match(TK_ELSE))
	{
		uint32_t jmp_else = emit_jump(OP_JMP);
		patch_jump(jmp_then);
		stmt();
		patch_jump(jmp_else);
	}
	else
		patch_jump(jmp_then);
	
	// pop the condition expression's result
	emit_byte(OP_POP);
}

static void and_()
{
	uint32_t jmp_param_pos = emit_jump(OP_JMP_IF_FALSE);
	emit_byte(OP_POP);
	parse_precedence(PREC_AND);
	patch_jump(jmp_param_pos);
}

static void or_()
{
	uint32_t left_is_false_jmp = emit_jump(OP_JMP_IF_FALSE);
	uint32_t jmp_out = emit_jump(OP_JMP);
	patch_jump(left_is_false_jmp);
	emit_byte(OP_POP);
	parse_precedence(PREC_OR);
	patch_jump(jmp_out);
}

/* var_declaration(): parse the var declaration statement
 * Bytecode form:<EXPRESSION> OP_DEFINE <OFFSET>, where
 *
 * <EXPRESSION>: bytecode form of the right-hand side expression
 * <OFFSET>: offset of the string object representing the identifier,
 * takes up three bytes
 * */
static void var_declaration()
{
	// parse the identifier and construct a string object (StringObj)
	// representing the identifier. The object is put in the constant pool.
	// offset -> offset of the object in the constant pool

	do {
		uint32_t offset = parse_identifier("Expect an identifier.");
		Token name = parser.consumed_identifier;
		if (match(TK_EQUAL))
			expression();
		else
			emit_byte(OP_NIL);
		declare_variable(name);
		define_variable(offset);
	} while (match(TK_COMMA));

	consume(TK_SEMICOLON, "Expect ';' after statement.");
}

static void add_local(Token name)
{
	/** TODO: handle the case in which number of items
	 * exceeds 255 (UINT8_MAX) */
	current->locals[current->local_count] = 
		(Local) { name, current->scope_depth };
	current->local_count++;
}

static void declare_variable(Token name)
{
	if (current->scope_depth == 0)
		return;
	for (Local * local_it = &current->locals[current->local_count - 1];
			local_it >= &current->locals; local_it--) 
	{
		if (local_it->depth != -1 && local_it->depth < current->scope_depth) {
			// does the iterator go 'outside' of the current scope?
			break;
		}

		if (identifier_equal(&name, &local_it->name)) {
			error(&name, "Redeclare variable inside scope.");
			return;
		}
	}
	add_local(name);
}

static void define_variable(uint32_t offset)
{
	if (current->scope_depth > 0)
		return;
	emit_param_inst(OP_DEFINE_GLOBAL, offset, 3);
}

/* parse_identifier: parse the identifier. If the current token is
 * not an identifier, emit an error message to stderr. Also, add a
 * string object representing the identifier to the constant pool.
 *
 * return value: offset of the identifier in the constant pool */
static uint32_t parse_identifier(const char * error_msg)
{
	consume(TK_IDENTIFIER, error_msg);
	parser.consumed_identifier = parser.prev;
	Value identifier = OBJ_VAL(
			*StringObj_construct(parser.prev.start, parser.prev.length)
			);
	return chunk_add_const(current_chunk(), identifier);
}

static void print_stmt()
{
	expression();
	consume(TK_SEMICOLON, "Expect a ';' after statement.");
	emit_byte(OP_PRINT);
}

static void expression_stmt()
{
	expression();
	consume(TK_SEMICOLON, "Expect a ';' after statement.");
	emit_byte(vm.repl ? OP_PRINT : OP_POP);
}

static void begin_scope()
{
	current->scope_depth++;
}

static void end_scope()
{
	/** Remove all current scope's local variable representations
	 *  from @current->locals.
	 */
	for (Local * local_it = &current->locals[current->local_count - 1];
			local_it >= &current->locals; local_it--) 
	{
		if (local_it->depth != -1 && local_it->depth < current->scope_depth)
			break;
		current->local_count--;

		// Local variables are looked up in the virtual machine's stack
		// so we need to add a pop instruction corresponding to each local variable
		emit_byte(OP_POP);
	}

	current->scope_depth--;
}

static void block_stmt()
{
	begin_scope();
	while (!(check(TK_RIGHT_BRACE) || check(TK_EOF)))
	{
		declaration();
	}

	consume(TK_RIGHT_BRACE, "Expect '}' at the end of block.");
	end_scope();
}

static void while_stmt()
{
	// condition expression
	consume(TK_LEFT_PAREN, "Expect '(' after 'while'.");
	uint32_t condition_pos = current_chunk()->size;
	expression();
	consume(TK_RIGHT_PAREN, "Expect ')' after expression.");

	// skip the loop statement if the expression is 'false'
	uint32_t out_of_loop_jmp = emit_jump(OP_JMP_IF_FALSE);
	emit_byte(OP_POP);

	// loop body
	stmt();
	emit_loop(condition_pos);
	patch_jump(out_of_loop_jmp);
	emit_byte(OP_POP);
}

// Desugar for-loop to while-loop 
static void for_stmt()
{
	// flow of a for-loop: initialization -> check condition -> execute body
	// -> increment -> jump back to checking condition

	begin_scope();
	consume(TK_LEFT_PAREN, "Expect '(' after 'for'.");

	// parse loop initializer
	if (match(TK_VAR))
		var_declaration();
	else
		expression_stmt();

	uint32_t condition_start = current_chunk()->size;

	// parse loop condition
	if (match(TK_SEMICOLON))
		emit_byte(OP_TRUE);
	else
	{
		expression();
		consume(TK_SEMICOLON, "Expect ';' after expression.");
	}

	int exit_loop = emit_jump(OP_JMP_IF_FALSE);
	emit_byte(OP_POP);	// discard the condition expression's value
	int enter_body = emit_jump(OP_JMP);
	int increment_start = current_chunk()->size;

	// parse the increment expression
	if (!check(TK_RIGHT_PAREN))
	{
		expression();
		emit_byte(OP_POP);
	}
	consume(TK_RIGHT_PAREN, "Expect ')' after increment expression.");

	emit_loop(condition_start);	// jump back to condition expression
	patch_jump(enter_body);	// reach body, patch the to-body jump
	if (!match(TK_SEMICOLON))
		stmt();
	else
		consume(TK_SEMICOLON, "Expect ';' after for-loop if there is no loop statement.");
	emit_loop(increment_start);	// jump back to increment expression if the body is executed
	patch_jump(exit_loop); // the body is parsed, patch the exit-loop jump
	emit_byte(OP_POP); 	// discard the condition's value if we are out of loop
	end_scope();
}

// parse statements that are not declaration type
static void stmt()
{
	if (match(TK_PRINT))
		print_stmt();
	else if (match(TK_LEFT_BRACE))
		block_stmt();
	else if (match(TK_IF))
		if_stmt();
	else if (match(TK_WHILE))
		while_stmt();
	else if (match(TK_FOR))
		for_stmt();
	else
		expression_stmt();
}

/* number: take the consumed token (@parser.prev) and 'transform' it into
 * a constant-load instruction */
static void number()
{
	double literal = strtod(parser.prev.start, NULL);
	emit_const_inst(NUMBER_VAL(literal));
}

static void string()
{
	/* Create a string object in heap memory */
	StringObj * str_obj = 
		StringObj_construct(parser.prev.start, parser.prev.length);
	emit_const_inst(OBJ_VAL(str_obj->obj));
}

static void literal()
{
	switch (parser.prev.type)
	{
		case TK_TRUE: emit_byte(OP_TRUE); break;
		case TK_FALSE: emit_byte(OP_FALSE); break;
		case TK_NIL: emit_byte(OP_NIL); break;
		default: return;
	}
}

/* unary(): parse the unary expression */
static void unary()
{
	Token op = parser.prev;
	parse_precedence(PREC_UNARY);

	switch (op.type)
	{
		case TK_MINUS:
			emit_byte(OP_NEGATE);
			break;
		case TK_BANG:
			emit_byte(OP_NOT);
			break;
		default:
			error(&op, "Invalid operation.");
	}
}


/* grouping(): parse the expression between two parentheses */
static void grouping()
{
	expression();
	consume(TK_RIGHT_PAREN, "Expect ')' after the expression.");
}

/* binary(): parse the binary operator and the next operand, one at a time */
static void binary()
{
	Token op = parser.prev;

	// get the precedence of the consumed operator
	Precedence op_prec = get_rule(op.type)->prec;
	parse_precedence(op_prec + 1);	// parse the next term

	switch (op.type)
	{
		/* We use chunk_append here because there are some cases where the operator and
		 * its operands do not lie on the same line. If an error occurs, we have to report
		 * the exact line on which a token is */

		case TK_PLUS: 	chunk_append(current_chunk(), OP_ADD, op.line); break;
		case TK_MINUS: 	chunk_append(current_chunk(), OP_SUBTRACT, op.line); break;
		case TK_STAR: 	chunk_append(current_chunk(), OP_MUL, op.line); break;
		case TK_SLASH: 	chunk_append(current_chunk(), OP_DIV, op.line); break;
		case TK_LESS: 	chunk_append(current_chunk(), OP_LESS, op.line); break;
		case TK_GREATER: chunk_append(current_chunk(), OP_GREATER, op.line); break;
		case TK_EQUAL_EQUAL: chunk_append(current_chunk(), OP_EQUAL, op.line); break;
		case TK_LESS_EQUAL:
			chunk_append(current_chunk(), OP_GREATER, op.line);
			chunk_append(current_chunk(), OP_NOT, op.line);
			break;
		case TK_GREATER_EQUAL:
			chunk_append(current_chunk(), OP_LESS, op.line);
			chunk_append(current_chunk(), OP_NOT, op.line);
			break;
		case TK_BANG_EQUAL:
			chunk_append(current_chunk(), OP_EQUAL, op.line);
			chunk_append(current_chunk(), OP_NOT, op.line);
			break;
		default: ;
	}

	parser.prev_prec = op_prec;
}

/* compile: parse the source and emit bytecodes.
 * return value: true if the compilation was successful */
bool compile(const char * source, Chunk * chunk)
{
	scanner_init(source);
	parser_init();
	Compiler compiler;
	compiler_init(&compiler);
	compiling_chunk = chunk;
	
 	// tell the parser to look at the very first token, but not consume it yet
	advance();

	while (!match(TK_EOF))
		declaration();
#ifdef BYTECODE_LOG
	disassemble_chunk(current_chunk(), "Disassemble bytecodes");
#endif
	scanner_free();
	compiler_free();
	end_compiler();
	return !parser.error;
}
