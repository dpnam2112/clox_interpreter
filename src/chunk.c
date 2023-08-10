#include "chunk.h"
#include "memory.h"
#include <stdint.h>

//Expand the chunk's bytecode array so that it can contain more n bytes
#define FIT_EXPAND_CHUNK(chunk, n)\
do {\
	while (chunk->size + n > chunk->capacity)\
	{\
		uint32_t new_cap = GROW_CAPACITY(chunk->capacity);\
		chunk->bytecodes = GROW_ARRAY(uint8_t, chunk->bytecodes, chunk->capacity, new_cap);\
		chunk->capacity = new_cap;\
	}\
} while (false)


void chunk_init(Chunk * chunk)
{
	chunk->size = 0;
	chunk->capacity = 0;
	chunk->bytecodes = NULL;
	chunk->current_line = 0;
	value_arr_init(&(chunk->constants));
}

void chunk_free(Chunk * chunk)
{
	if (chunk->bytecodes != NULL)
	{
		FREE_ARRAY(uint8_t, chunk->bytecodes, chunk->capacity);
	}

	value_arr_free(&(chunk->constants));
	chunk_init(chunk);
}

void chunk_append(Chunk * chunk, uint8_t byte, uint16_t line)
{
	if (line > 0 && line != chunk->current_line)
	{
		// Add line metadata (3 bytes)
		FIT_EXPAND_CHUNK(chunk, 3);
		chunk->bytecodes[chunk->size] = META_LINE_NUM; 
		memcpy(&(chunk->bytecodes[chunk->size + 1]), &line, 2);
		chunk->size += 3;
		chunk->current_line = line;
	}

	FIT_EXPAND_CHUNK(chunk, 1);
	chunk->bytecodes[chunk->size] = byte;
	chunk->size++;
}

int get_inst_size(Opcode type)
{
	switch (type)
	{
	case OP_CONST:
		return 2;
	case OP_JMP:
	case OP_JMP_IF_FALSE:
	case OP_LOOP:
		return 3;
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
	case OP_DEFINE_GLOBAL:
	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL:
	case OP_CONST_LONG:
		return 4;
	default:
		return 1;
	}
}

uint16_t chunk_get_line(Chunk * chunk, uint32_t index)
{
	uint16_t line;
	uint32_t i;
	for (i = 0; i < index && i < chunk->size;)
	{
		if (chunk->bytecodes[i] == META_LINE_NUM)
			memcpy(&line, &(chunk->bytecodes[i + 1]), 2);
		// Jump to opcode of the next instruction
		i += get_inst_size(chunk->bytecodes[i]);
	}

	return line;
}

void chunk_append_bytes(Chunk * chunk, void * bytes, int n)
{
	FIT_EXPAND_CHUNK(chunk, n);
	memcpy(&(chunk->bytecodes[chunk->size]), bytes, n);
	chunk->size += n;
}

uint32_t chunk_add_const(Chunk * chunk, Value value)
{
	if (chunk->constants.size + 1 > CONST_POOL_LIMIT)
	{
		return 0;	// dummy offset
	}

	value_arr_append(&chunk->constants, value);
	return chunk->constants.size - 1;
}

void chunk_write_load_const(Chunk * chunk, Value value, uint16_t line)
{
	// position of the value in constant pool
	uint32_t const_offset = 0;
	const_offset = chunk_add_const(chunk, value);

	// add instruction's opcode
	chunk_append(chunk,
		(const_offset <= UINT8_MAX) ? OP_CONST : OP_CONST_LONG, line);

	// parameter of OP_CONST_LONG takes up 3 bytes
	chunk_append_bytes(chunk, &const_offset,
			(const_offset <= UINT8_MAX) ? 1 : 3);
}

uint32_t chunk_get_const_pool_size(Chunk * chunk) {
	return chunk->constants.size;
}
