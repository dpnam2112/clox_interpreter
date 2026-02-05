#ifndef COMPILER_H
#define COMPILER_H

#include "object.h"

typedef enum {
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
	PREC_DOT,
	PREC_CALL,
	PREC_PRIMARY,		// highest precedence
}
Precedence;

ClosureObj * compile(const char * source);
bool mark_compiler_roots();

#endif
