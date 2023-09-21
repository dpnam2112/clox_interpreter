#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "scanner.h"
#include "vm.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include <stdlib.h>
#include <math.h>

typedef struct IntArr
{
	int *head;
	int size;
	int capacity;
} IntArr;

typedef struct Parser
{
	Token prev;
	Token current;
	Token consumed_identifier; // identifier that is consumed recently
	bool error;				   // was there any parse error occured?
	bool panic;				   // should the parser enter panic mode?
	Precedence prev_prec;

	// These fields are used to store positions of each 
	// jump statement (breaks and continues).
	IntArr * breaks;
	IntArr * continues;
} Parser;

typedef struct Local
{
	Token name;
	int depth;
	bool captured;
} Local;

typedef enum FunctionType
{
	TYPE_FUNCTION,
	TYPE_SCRIPT,
} FunctionType;

/** Upvalue: used to resolve variables that lie outside
 * of the current function's scope.
 *
 * @index: index of the variable in the enclosing function's stack array
 * @local: indicate whether the upvalue refer to an object belonging to the enclosing closure/function
 * @long_offset: indicate whether the @index is one-byte long (i.e, 0 <= @index <= UINT8_MAX).
 */
typedef struct Upvalue
{
	uint32_t index;
	bool local;
	bool long_offset;
}
Upvalue;

// This struct is used to compile function body.
// After the function body is compiled, end_compiler()
// is called to return a function object that represents
// the compiled function.
// Top-level script can be considered as a function, so
// this struct is also used to compile top-level script.
typedef struct Compiler
{
	// @enclosing: refers to the compiler that compiles
	// the function enclosing the current function
	struct Compiler * enclosing;
	FunctionObj *function;
	FunctionType func_type;

	// This field (@locals) is used to resolve variables
	// inside scopes.
	Local locals[UINT8_MAX];
	int local_count;
	int scope_depth;

	Upvalue upvalues[UINT8_MAX];
	uint8_t upval_count;
} Compiler;

Parser parser;
Chunk *compiling_chunk;
Compiler * current = NULL;

static void int_arr_init(IntArr *arr)
{
	arr->head = ALLOCATE(int, 10);
	arr->size = 0;
	arr->capacity = 10;
}

static void int_arr_free(IntArr *arr)
{
	FREE_ARRAY(int, arr->head, arr->capacity);
	arr->capacity = arr->size = 0;
}

static void emit_error_msg(Token *tk_error, const char *msg)
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

static void error(Token *tk_error, const char *msg)
{
	parser.error = true;
	emit_error_msg(tk_error, msg);
}


static void int_arr_append(IntArr *arr, int item)
{
	if (arr->size + 1 == arr->capacity)
	{
		int old_cap = arr->capacity;
		arr->capacity = GROW_CAPACITY(old_cap);
		arr->head = GROW_ARRAY(int, arr->head, old_cap, arr->capacity);
	}
	arr->head[arr->size++] = item;
}

static void add_local(Token name)
{
	/** TODO: handle the case in which number of items
	 * exceeds 255 (UINT8_MAX) */
	current->locals[current->local_count] =
		(Local){name, current->scope_depth, false};
	current->local_count++;
}

/** @add_upvalue: add an upvalue to the upvalue array
 * @param compiler: compiler that compiles the current function / closure object
 * @param index: index of the upvalue in the enclosing function's array of upvalues if local is false
 * 				 index of the value referenced by the upvalue in the virtual machine's stack
 * @param local: indicate whether the value referenced is inside stack or another function's upvalue array
 * @return: index of the added upvalue
 */
static uint32_t add_upvalue(Compiler * compiler, int index, bool local)
{
	int upval_count = compiler->upval_count;

	if (upval_count > 1 << 2 * sizeof(uint8_t))
	{
		error(&parser.prev, "[Memory error] Too many upvalues.");
		return upval_count;
	}

	for (int i = upval_count - 1; i >= 0; i--)
	{
		if (compiler->upvalues[i].index == index && compiler->upvalues[i].local == local)
			return i;
	}

	bool long_offset = (compiler->upval_count > UINT8_MAX);
	compiler->upvalues[compiler->upval_count] = (Upvalue) { index, local, long_offset };
	return compiler->upval_count++;
}

/** compiler_init: initialize a new compiler used to
 * compile function body.
 */
static void compiler_init(Compiler *compiler, FunctionType type)
{
	compiler->function = FunctionObj_construct();
	compiler->func_type = type;
	compiler->enclosing = current;
	compiler->scope_depth = compiler->local_count = 0;
	if (type != TYPE_SCRIPT) {
		compiler->function->name = StringObj_construct(
			parser.consumed_identifier.start,
			parser.consumed_identifier.length
		);
	}
	compiler->upval_count = 0;

	current = compiler;

	// dummy local
	add_local((Token){TK_EOF, "", 0, 0});
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
	parser.breaks = NULL;
	parser.continues = NULL;
}

void parser_free()
{
}

static void add_break(uint32_t jmp_param)
{
	int_arr_append(parser.breaks, jmp_param);
}

static void add_continue(uint32_t jmp_param)
{
	int_arr_append(parser.continues, jmp_param);
}

static Chunk *current_chunk()
{
	return &current->function->chunk;
}


static void emit_byte(uint8_t byte)
{
	chunk_append(current_chunk(), byte, parser.prev.line);
}

static void emit_bytes(void *bytes, uint8_t byte_count)
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

static uint32_t identifier_constant(Token *tk)
{
	StringObj *identifier = StringObj_construct(tk->start, tk->length);
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
		case TK_BREAK:
		case TK_CONTINUE:
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
static void consume(TokenType type, const char *msg)
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
	emit_byte(OP_NIL);
	emit_byte(OP_RETURN);
}

static ClosureObj * end_compiler()
{
	emit_return();
	FunctionObj *function = current->function;
	function->upval_count = current->upval_count;
#ifdef BYTECODE_DUMP
#endif
#ifdef DBG_DISASSEMBLE
	/* Print the instruction to be executed */
	disassemble_chunk(&function->chunk,
					  function->name == NULL ? "script" : function->name->chars);
#endif

	// continue compiling the enclosing function
	current = current->enclosing;
	return ClosureObj_construct(function);
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
static void fun_declaration();
static void if_stmt();
static void while_stmt();
static void for_stmt();
static void break_stmt();
static uint32_t parse_identifier(const char *error_msg);
static void declare_variable(Token name);
static void define_variable(uint32_t offset);
static uint8_t parameter_list();
static void and_();
static void or_();
static void call();

typedef void (*ParseFn)(); // pointer to a parsing function

typedef struct ParseRule
{
	ParseFn prefix;	 // function called if the current operation is prefix
	ParseFn infix;	 // function called if the current operation is infix
	Precedence prec; // precedence associated to the current infix operation
} ParseRule;

/* parse_rules: a mapping from operators to appropiate parsing rules */
ParseRule parse_rules[] = {
	[TK_TRUE] = {literal, NULL, PREC_PRIMARY},
	[TK_FALSE] = {literal, NULL, PREC_PRIMARY},
	[TK_NIL] = {literal, NULL, PREC_PRIMARY},
	[TK_LEFT_PAREN] = {grouping, call, PREC_CALL},
	[TK_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
	[TK_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
	[TK_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
	[TK_COMMA] = {NULL, NULL, PREC_NONE},
	[TK_DOT] = {NULL, NULL, PREC_NONE},
	[TK_MINUS] = {unary, binary, PREC_TERM},
	[TK_PLUS] = {NULL, binary, PREC_TERM},
	[TK_SEMICOLON] = {NULL, NULL, PREC_NONE},
	[TK_SLASH] = {NULL, binary, PREC_FACTOR},
	[TK_STAR] = {NULL, binary, PREC_FACTOR},
	[TK_BANG] = {unary, NULL, PREC_UNARY},
	[TK_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
	[TK_EQUAL] = {NULL, assignment, PREC_ASSIGNMENT},
	[TK_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
	[TK_GREATER] = {NULL, binary, PREC_COMPARISON},
	[TK_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
	[TK_LESS] = {NULL, binary, PREC_COMPARISON},
	[TK_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
	[TK_IDENTIFIER] = {variable, NULL, PREC_NONE},
	[TK_STRING] = {string, NULL, PREC_PRIMARY},
	[TK_NUMBER] = {number, NULL, PREC_PRIMARY},
	[TK_AND] = {NULL, and_, PREC_AND},
	[TK_CLASS] = {NULL, NULL, PREC_NONE},
	[TK_ELSE] = {NULL, NULL, PREC_NONE},
	[TK_FOR] = {NULL, NULL, PREC_NONE},
	[TK_FUN] = {NULL, NULL, PREC_NONE},
	[TK_IF] = {NULL, NULL, PREC_NONE},
	[TK_OR] = {NULL, or_, PREC_OR},
	[TK_PRINT] = {NULL, NULL, PREC_NONE},
	[TK_RETURN] = {NULL, NULL, PREC_NONE},
	[TK_SUPER] = {NULL, NULL, PREC_NONE},
	[TK_THIS] = {NULL, NULL, PREC_NONE},
	[TK_VAR] = {NULL, NULL, PREC_NONE},
	[TK_WHILE] = {NULL, NULL, PREC_NONE},
	[TK_ERROR] = {NULL, NULL, PREC_NONE},
	[TK_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *get_rule(TokenType op)
{
	return &(parse_rules[op]);
}

static bool identifier_equal(Token *tk_1, Token *tk_2)
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

	advance(); // consume the prefix operator or a literal
	ParseFn prefix_rule = get_rule(parser.prev.type)->prefix;

	if (prefix_rule == NULL)
	{
		error(&parser.prev, "Expect an expression.");
		return;
	}

	prefix_rule(); // call the appropriate function to parse the prefix expression or literal
	parser.prev_prec = get_rule(parser.prev.type)->prec;

	while (get_rule(parser.current.type)->prec >= prec && !parser.panic)
	{
		if (get_rule(parser.current.type)->prec == PREC_PRIMARY)
		{
			error(&parser.current, "Expect an operator here.");
			break;
		}

		advance(); // consume the infix operator
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

static int resolve_local(Compiler *compiler, Token *name)
{
	for (int i = compiler->local_count - 1; i >= 0; i--)
	{
		Local *current_local = &compiler->locals[i];
		if (identifier_equal(name, &current_local->name))
			return i;
	}
	return -1;
}

static uint32_t resolve_upvalue(Compiler * compiler,  Token * name)
{
	if (compiler->enclosing == NULL)
		return -1;

	// if there is no variable named @name declared inside
	// the current function, we need to check in the enclosing functions 

	// We consider if the variable is not found in any enclosing functions,
	// it is global by default

	uint32_t enclosing_local = resolve_local(compiler->enclosing, name);
	if (enclosing_local != -1) {
		compiler->enclosing->locals[enclosing_local].captured = true;
		return add_upvalue(compiler, enclosing_local, true);
	}

	uint32_t enclosing_upval = resolve_upvalue(compiler->enclosing, name);
	if (enclosing_upval != -1)
		return add_upvalue(compiler, enclosing_upval, false);
	return -1;
}


static void call()
{
	uint8_t param_count = parameter_list();
	if (param_count > UINT8_MAX)
		error(&parser.prev, "Exceed limit of number of parameters.");
	emit_byte(OP_CALL);
	emit_byte(param_count);
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
	Token name = parser.consumed_identifier; // used to resolve variable's name

	parse_precedence(PREC_ASSIGNMENT);

	uint32_t stack_index = resolve_local(current, &name);
	uint32_t upvalue_index = 0;

	if (stack_index != -1 && stack_index <= UINT8_MAX)
		emit_param_inst(OP_SET_LOCAL, stack_index, 1);
	else if (stack_index != -1)
		emit_param_inst(OP_SET_LOCAL_LONG, stack_index, LONG_LOCAL_OFFSET_SIZE);
	else if ((upvalue_index = resolve_upvalue(current, &name)) != -1 && upvalue_index <= UINT8_MAX)
		emit_param_inst(OP_SET_UPVAL, upvalue_index, 1);
	else if (upvalue_index != -1)
		emit_param_inst(OP_SET_UPVAL_LONG, upvalue_index, LONG_UPVAL_OFFSET_SIZE);
	else if (iden_offset <= UINT8_MAX)
		emit_param_inst(OP_SET_GLOBAL, iden_offset, 1);
	else
		emit_param_inst(OP_SET_GLOBAL_LONG, iden_offset, LONG_CONST_OFFSET_SIZE);
}

static void variable()
{
	parser.consumed_identifier = parser.prev;
	uint32_t offset = identifier_constant(&parser.prev);

	// if the next operation is assignment, no need to load the variable
	// to the stack
	if (parser.current.type == TK_EQUAL)
		return;

	uint32_t stack_index = resolve_local(current, &parser.prev);
	uint32_t upvalue_index;
	if (stack_index != -1 && stack_index <= UINT8_MAX)
		emit_param_inst(OP_GET_LOCAL, stack_index, 1);
	else if (stack_index != -1)
		emit_param_inst(OP_GET_LOCAL_LONG, stack_index, LONG_LOCAL_OFFSET_SIZE);
	else if ((upvalue_index = resolve_upvalue(current, &parser.prev)) != -1 && upvalue_index <= UINT8_MAX)
		emit_param_inst(OP_GET_UPVAL, upvalue_index, 1);
	else if (upvalue_index != -1)
		emit_param_inst(OP_GET_UPVAL_LONG, upvalue_index, LONG_UPVAL_OFFSET_SIZE);
	else if (offset <= UINT8_MAX)
		emit_param_inst(OP_GET_GLOBAL, offset, 1);
	else
		emit_param_inst(OP_GET_GLOBAL, offset, LONG_CONST_OFFSET_SIZE);
}

static void declaration()
{
	if (match(TK_VAR))
		var_declaration();
	else if (match(TK_FUN))
		fun_declaration();
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
	return (uint16_t)jump_dist;
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

	do
	{
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

static void begin_scope()
{
	current->scope_depth++;
}

static void end_scope()
{
	/** Remove all current scope's local variable representations
	 *  from @current->locals.
	 */
	for (Local *local_it = &current->locals[current->local_count - 1];
		 local_it >= &current->locals; local_it--)
	{
		if (local_it->depth != -1 && local_it->depth < current->scope_depth)
			break;
		current->local_count--;

		// Local variables are looked up in the virtual machine's stack
		// so we need to add a pop instruction corresponding to each local variable
		if (local_it->captured)
		{
			emit_byte(OP_CLOSE_UPVAL);
		}
		else
		{
			emit_byte(OP_POP);
		}
	}

	current->scope_depth--;
}

static uint8_t parameter_list()
{
	uint8_t param_count = 0;
	if (!check(TK_RIGHT_PAREN))
	{
		do {
			expression();
			param_count++;
		} while (match(TK_COMMA));
	}

	consume(TK_RIGHT_PAREN, "Expect ')' after list of parameters.");
	return param_count;
}


static void function(FunctionType type)
{
	Compiler compiler;
	compiler_init(&compiler, type);
	begin_scope();

	consume(TK_LEFT_PAREN, "Expect '(' after function name.");
	uint8_t param_count = 0;
	if (!check(TK_RIGHT_PAREN))
	{
		do
		{
			if (param_count == UINT8_MAX)
				error(&parser.prev, "Exceed limit of number of parameters.");
			uint32_t name = parse_identifier("Expect parameter's name.");
			declare_variable(parser.consumed_identifier);
			define_variable(name);
			param_count++;
		} while (match(TK_COMMA));
	}
	consume(TK_RIGHT_PAREN, "Expect ')' after parameter list.");
	consume(TK_LEFT_BRACE, "Expect '{' after ')'.");
	block_stmt();
	end_scope();
	
	ClosureObj * closure = end_compiler();
	closure->function->arity = param_count;
	emit_const_inst(OBJ_VAL(*closure));

	for (int i = 0; i < compiler.upval_count; i++)
	{
		Upvalue upvalue = compiler.upvalues[i];
		emit_byte(upvalue.local | (upvalue.long_offset << 1));
		emit_bytes(&(upvalue.index), (upvalue.long_offset) ? 2 : 1);
	}
}

static void fun_declaration()
{
	uint32_t name = parse_identifier("Expect function name.");
	declare_variable(parser.consumed_identifier);
	function(TYPE_FUNCTION);
	define_variable(name);
}

static void declare_variable(Token name)
{
	if (current->scope_depth == 0)
		return;
	for (Local *local_it = &current->locals[current->local_count - 1];
		 local_it >= &current->locals; local_it--)
	{
		if (local_it->depth != -1 && local_it->depth < current->scope_depth)
		{
			// does the iterator go 'outside' of the current scope?
			break;
		}

		if (identifier_equal(&name, &local_it->name))
		{
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
	if (offset <= UINT8_MAX)
		emit_param_inst(OP_DEFINE_GLOBAL, offset, 1);
	else
		emit_param_inst(OP_DEFINE_GLOBAL_LONG, offset, 3);
}

/* parse_identifier: parse the identifier. If the current token is
 * not an identifier, emit an error message to stderr. Also, add a
 * string object representing the identifier to the constant pool.
 *
 * return value: offset of the identifier in the constant pool */
static uint32_t parse_identifier(const char *error_msg)
{
	consume(TK_IDENTIFIER, error_msg);
	parser.consumed_identifier = parser.prev;
	Value identifier = OBJ_VAL(
		*StringObj_construct(parser.prev.start, parser.prev.length));
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

static void break_stmt()
{
	if (parser.breaks == NULL)
	{
		// parser is not inside of a loop
		error(&parser.prev, "use of 'break' outside loop.");
		consume(TK_SEMICOLON, "Expect ';' after 'break'.");
		return;
	}

	uint32_t brk = emit_jump(OP_JMP);
	add_break(brk);
	consume(TK_SEMICOLON, "Expect ';' after 'break'.");
}

static void patch_break()
{
	if (parser.breaks == NULL)
		return;
	for (int i = 0; i < parser.breaks->size; i++)
		patch_jump(parser.breaks->head[i]);
}

static void patch_continue()
{
	if (parser.continues == NULL)
		return;
	for (int i = 0; i < parser.continues->size; i++)
		patch_jump(parser.continues->head[i]);
}

static void continue_stmt()
{
	if (parser.continues == NULL)
		error(&parser.prev, "use of 'continue' outside loop.");
	else
	{
		uint32_t jmp_param = emit_jump(OP_JMP);
		add_continue(jmp_param);
	}

	consume(TK_SEMICOLON, "Expect ';' after 'continue'.");
}

static void while_stmt()
{
	IntArr *outer_loop_brk = parser.breaks;
	IntArr *outer_loop_continue = parser.continues;
	IntArr current_loop_brk, current_loop_continue;
	int_arr_init(&current_loop_brk);
	int_arr_init(&current_loop_continue);
	parser.breaks = &current_loop_brk;
	parser.continues = &current_loop_continue;

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

	// emit instruction to jump back to the condition evaluation
	// patch all previous jumps (including breaks and continues)
	patch_continue();
	emit_loop(condition_pos);
	patch_jump(out_of_loop_jmp);
	patch_break();
	emit_byte(OP_POP);

	parser.breaks = outer_loop_brk;
	parser.continues = outer_loop_continue;
	int_arr_free(&current_loop_brk);
	int_arr_free(&current_loop_continue);
}

// Desugar for-loop to while-loop
static void for_stmt()
{
	IntArr *outer_loop_brk = parser.breaks;
	IntArr *outer_loop_continue = parser.continues;
	IntArr current_loop_brk, current_loop_continue;
	int_arr_init(&current_loop_brk);
	int_arr_init(&current_loop_continue);
	parser.breaks = &current_loop_brk;
	parser.continues = &current_loop_continue;

	// flow of a for-loop: initialization -> check condition -> execute body
	// -> increment -> jump back to checking condition

	begin_scope();
	consume(TK_LEFT_PAREN, "Expect '(' after 'for'.");

	// parse loop initializer
	if (match(TK_VAR))
		var_declaration();
	else if (!match(TK_SEMICOLON))
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
	emit_byte(OP_POP); // discard the condition expression's value
	int enter_body = emit_jump(OP_JMP);
	int increment_start = current_chunk()->size;

	// parse the increment expression
	if (!check(TK_RIGHT_PAREN))
	{
		expression();
		emit_byte(OP_POP);
	}
	consume(TK_RIGHT_PAREN, "Expect ')' after increment expression.");

	emit_loop(condition_start); // jump back to condition expression
	patch_jump(enter_body);		// reach body, patch the to-body jump
	if (!check(TK_SEMICOLON))
	{
		stmt();
		patch_continue();
	}
	else
		consume(TK_SEMICOLON, "Expect ';' after for-loop if there is no loop statement.");
	emit_loop(increment_start); // jump back to increment expression if the body is executed
	patch_jump(exit_loop);		// the body is parsed, patch the exit-loop jump
	patch_break();
	emit_byte(OP_POP); // discard the condition's value if we are out of loop
	end_scope();

	parser.breaks = outer_loop_brk;
	parser.continues = outer_loop_continue;
	int_arr_free(&current_loop_brk);
	int_arr_free(&current_loop_continue);
}

static void return_stmt()
{
	if (current->enclosing == NULL)
	{
		error(&parser.prev, "'return' outside function.");
	}

	if (!check(TK_SEMICOLON))
		expression();
	else
		emit_byte(OP_NIL);
	consume(TK_SEMICOLON, "Expect ';' after statement.");
	emit_byte(OP_RETURN);
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
	else if (match(TK_BREAK))
		break_stmt();
	else if (match(TK_CONTINUE))
		continue_stmt();
	else if (match(TK_RETURN))
		return_stmt();
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
	StringObj *str_obj =
		StringObj_construct(parser.prev.start, parser.prev.length);
	emit_const_inst(OBJ_VAL(str_obj->obj));
}

static void literal()
{
	switch (parser.prev.type)
	{
	case TK_TRUE:
		emit_byte(OP_TRUE);
		break;
	case TK_FALSE:
		emit_byte(OP_FALSE);
		break;
	case TK_NIL:
		emit_byte(OP_NIL);
		break;
	default:
		return;
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
	parse_precedence(op_prec + 1); // parse the next term

	switch (op.type)
	{
		/* We use chunk_append here because there are some cases where the operator and
		 * its operands do not lie on the same line. If an error occurs, we have to report
		 * the exact line on which a token is */

	case TK_PLUS:
		chunk_append(current_chunk(), OP_ADD, op.line);
		break;
	case TK_MINUS:
		chunk_append(current_chunk(), OP_SUBTRACT, op.line);
		break;
	case TK_STAR:
		chunk_append(current_chunk(), OP_MUL, op.line);
		break;
	case TK_SLASH:
		chunk_append(current_chunk(), OP_DIV, op.line);
		break;
	case TK_LESS:
		chunk_append(current_chunk(), OP_LESS, op.line);
		break;
	case TK_GREATER:
		chunk_append(current_chunk(), OP_GREATER, op.line);
		break;
	case TK_EQUAL_EQUAL:
		chunk_append(current_chunk(), OP_EQUAL, op.line);
		break;
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
	default:;
	}

	parser.prev_prec = op_prec;
}

// compile: parse the source and emit bytecodes.
// return value: Closure object that contains the top-level code
ClosureObj * compile(const char *source)
{
	scanner_init(source);
	parser_init();
	Compiler compiler;
	compiler_init(&compiler, TYPE_SCRIPT);

	// tell the parser to look at the very first token, but not consume it yet
	advance();

	while (!match(TK_EOF))
		declaration();
	scanner_free();
	parser_free();
	ClosureObj * closure = end_compiler();
	return parser.error ? NULL : closure;
}
