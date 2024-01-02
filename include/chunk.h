#ifndef CHUNK_H
#define CHUNK_H

#include "value.h"

#define INIT_SIZE 8

/** Some instructions take an offset of an object or something as a parameter.
 *  e.g: OP_CONST, OP_CONST_LONG, OP_GET_VAL, OP_GET_VAL_LONG,...
 *  The following constants define the size of the parameters of *_LONG instructions.
 */

#define LONG_LOCAL_OFFSET_SIZE 3
#define LONG_UPVAL_OFFSET_SIZE 2
#define LONG_CONST_OFFSET_SIZE 3

typedef enum
{
	OP_CONST,
	OP_CONST_LONG,
	OP_RETURN,
	OP_NEGATE,
	OP_EXIT,
	OP_NOT,
	OP_PRINT,
	OP_POP,
	OP_DEFINE_GLOBAL,
	OP_DEFINE_GLOBAL_LONG,
	OP_GET_GLOBAL,
	OP_GET_GLOBAL_LONG,
	OP_SET_GLOBAL,
	OP_SET_GLOBAL_LONG,
	OP_GET_UPVAL,
	OP_GET_UPVAL_LONG,
	OP_SET_UPVAL,
	OP_SET_UPVAL_LONG,
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
	OP_GET_LOCAL_LONG,
	OP_SET_LOCAL,
	OP_SET_LOCAL_LONG,
	OP_JMP_IF_FALSE,
	OP_JMP,
	OP_LOOP,
	OP_CALL,
	OP_CLOSURE,
	OP_CLOSURE_LONG,
	OP_CLOSE_UPVAL,
	META_LINE_NUM,
} Opcode;

/** Used to keep track of line numbers of bytecodes.
 * */
typedef struct BytecodeLine
{
	uint32_t line;	// line number
	uint32_t pos;	// bytecode position
	struct BytecodeLine * next;
} BytecodeLine;

// Dynamic array of bytecodes
typedef struct
{
	uint8_t * bytecodes;
	uint32_t size;
	uint32_t capacity;
	ValueArr constants;	// constant values
	uint16_t current_line;	// current line number
	BytecodeLine * line_tracker;
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
