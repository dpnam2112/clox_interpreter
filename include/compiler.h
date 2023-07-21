#ifndef COMPILER_H
#define COMPILER_H

#include "common.h"
#include "scanner.h"
#include "chunk.h"

typedef struct Parser
{
	Token prev;
	Token current;
	bool error;	// was there any error occured?
	bool panic;	// should the parser enter panic mode?
}
Parser;

typedef enum
{
	// Precedence is declared in order, from the lowest precedence
	// to the highest precedence
	PREC_NONE,
	PREC_ASSIGNMENT,	// lowest precedence
	PREC_OR,
	PREC_AND,
	PREC_EQUALITY,
	PREC_COMPARISON,
	PREC_TERM,
	PREC_FACTOR,
	PREC_UNARY,
	PREC_CALL,
	PREC_PRIMARY,		// highest precedence
}
Precedence;

bool compile(const char * source, Chunk * chunk);

#endif
