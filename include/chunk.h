#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "memory.h"
#include "value.h"

#define INIT_SIZE 8

/* Define size of each instruction type */
#define OP_CONST_SZ 2
#define OP_CONST_LONG_SZ 4
#define META_LINE_NUM_SZ 3
#define OP_RETURN_SZ 1
#define OP_NEGATE_SZ 1
#define OP_ADD_SZ 1
#define OP_SUBTRACT_SZ 1
#define OP_MUL_SZ 1
#define OP_DIV_SZ 1

typedef enum
{
	/* OP_CONST_*: Load a constant from constant pool to the vm's stack */
	OP_CONST,	// constant offsets that take up one byte
	OP_CONST_LONG,	// constant offsets that take up two bytes
	OP_RETURN,
	OP_NEGATE,	// Negate the top value of the vm's stack and return the negated value
	OP_EXIT,	// Exit program
	OP_AND,
	OP_OR,
	OP_NOT,
	OP_PRINT,
	OP_POP,
	OP_DEFINE_GLOBAL,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,

	OP_TRUE,
	OP_FALSE,
	OP_NIL,
	OP_LESS,
	OP_GREATER,
	OP_EQUAL,

	/* Simple arithmetic operations: addition, subtraction, multiplication, division
	 * these instructions take two values on the top of the vm's stack as their parameters */
	OP_ADD,
	OP_SUBTRACT,
	OP_MUL,
	OP_DIV,

	/* Metadata */
	META_LINE_NUM,		// line numbers
} Opcode;

// Dynamic array of bytecodes
typedef struct
{
	uint8_t * bytecodes;
	uint32_t size;
	uint32_t capacity;
	ValueArr constants;	// constant values
	uint16_t current_line;	// current line number

} Chunk;

void chunk_init(Chunk * chunk);
void chunk_free(Chunk * chunk);

/* chunk_append: append a byte to chunk
 * @byte: bytecode to be written
 * @line: line associated to the bytecode
 * */
void chunk_append(Chunk * chunk, uint8_t byte, uint16_t line);

/* Add a sequence of @n bytes to the chunk */
void chunk_append_bytes(Chunk * chunk, void * bytes, int n);

/* add_const: add 'value' to chunk->constants and return
 * index of the added value */
uint32_t chunk_add_const(Chunk * chunk, Value value);

/* write a load instruction that loads @value from constant pool to bytecode array */
void chunk_write_load_const(Chunk * chunk, Value value, uint16_t line);

/* Get the line where the bytecode at index is interpreted  */
uint16_t chunk_get_line(Chunk * chunk, uint32_t index);
#endif
