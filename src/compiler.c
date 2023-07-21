#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "scanner.h"
#include "vm.h"
#include <stdlib.h>
#include <math.h>

Parser parser;
Chunk * compiling_chunk;


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
		fprintf(stderr, ", at end: %s.\n", msg);
	else if (tk_error->type != TK_ERROR)
		fprintf(stderr, ", at token '%.*s': %s.\n", tk_error->length,
				tk_error->start, msg); 
	else
		fprintf(stderr, ": %.*s.\n", tk_error->length, tk_error->start);
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
	emit_return();
}

static void unary();
static void binary();
static void grouping();
static void number();

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
  [TK_BANG]          = {NULL,     NULL,   PREC_NONE},
  [TK_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TK_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TK_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
  [TK_GREATER]       = {NULL,     NULL,   PREC_NONE},
  [TK_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TK_LESS]          = {NULL,     NULL,   PREC_NONE},
  [TK_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TK_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TK_STRING]        = {NULL,     NULL,   PREC_NONE},
  [TK_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TK_AND]           = {NULL,     NULL,   PREC_NONE},
  [TK_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TK_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TK_FALSE]         = {NULL,     NULL,   PREC_NONE},
  [TK_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TK_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TK_IF]            = {NULL,     NULL,   PREC_NONE},
  [TK_NIL]           = {NULL,     NULL,   PREC_NONE},
  [TK_OR]            = {NULL,     NULL,   PREC_NONE},
  [TK_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TK_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TK_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TK_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TK_TRUE]          = {NULL,     NULL,   PREC_NONE},
  [TK_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TK_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TK_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TK_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule * get_rule(TokenType op)
{
	return &(parse_rules[op]);
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
		error(&parser.prev, "Expect an expression");
		return;
	}

	prefix_rule();	// call the appropriate function to parse the prefix expression or literal
	while (get_rule(parser.current.type)->prec >= prec)
	{
		advance();	// consume the infix operator
		// find the appropriate function to parse the second operand
		ParseFn infix_rule = get_rule(parser.prev.type)->infix;
		infix_rule();
	}
}


static void expression()
{
	parse_precedence(PREC_ASSIGNMENT);
}

/* number: take the consumed token (@parser.prev) and 'transform' it into
 * a constant-load instruction */
static void number()
{
	double literal = strtod(parser.prev.start, NULL);
	chunk_write_load_const(current_chunk(), literal, parser.prev.line);
}

/* unary(): parse the unary expression */
static void unary()
{
	Token op = parser.prev;
	parse_precedence(PREC_UNARY);

	switch (op.type)
	{
		case TK_MINUS:
			chunk_append(current_chunk(), OP_NEGATE, op.line);
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
		case TK_PLUS: 	chunk_append(current_chunk(), OP_ADD, op.line); break;
		case TK_MINUS: 	chunk_append(current_chunk(), OP_SUBTRACT, op.line); break;
		case TK_STAR: 	chunk_append(current_chunk(), OP_MUL, op.line); break;
		case TK_SLASH: 	chunk_append(current_chunk(), OP_DIV, op.line); break;
		default:
	}
}

/* compile: parse the source and emit bytecodes.
 * return value: true if the compilation was successful */
bool compile(const char * source, Chunk * chunk)
{
	scanner_init(source);
	compiling_chunk = chunk;
	
 	// tell the parser to look at the very first token, but not consume it yet
	advance();

	expression();
#ifdef BYTECODE_LOG
	disassemble_chunk(current_chunk(), "Disassemble bytecodes");
#endif
	emit_return();

	scanner_free();
	end_compiler();
	return !parser.error;
}

