#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "memory.h"
#include "value.h"

#define INIT_SIZE 8

typedef enum
{
	OP_CONST,
	OP_CONST_LONG,
	OP_RETURN,
	OP_NEGATE,
	OP_EXIT,
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
	OP_ADD,
	OP_SUBTRACT,
	OP_MUL,
	OP_DIV,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_JMP_IF_FALSE,
	OP_JMP,
	OP_LOOP,
	META_LINE_NUM,
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

uint32_t chunk_get_const_pool_size(Chunk * chunk);
#endif
