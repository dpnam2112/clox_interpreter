#include "debug.h"
#include "chunk.h"

void disassemble_chunk(Chunk * chunk, const char * name)
{
	printf("== %s ==\n", name);
	for (int offset = 0; offset < chunk->size;)
	{
		offset = disassemble_inst(chunk, offset);
	}
}

// simple_instruction: print out the instruction opcode and return 
// the offset of the next one.
int simple_instruction(const char * name, int offset)
{
	printf("%s\n", name);
	return offset + 1;
}

/* const_instruction: print the instruction and return the offset of the
 * next instruction. */
int const_instruction(const char * name, Chunk * chunk, int offset)
{
	// offset of the constant value in chunk->values
	uint8_t const_offset = chunk->bytecodes[offset + 1];
	printf("%-16s %4d '", name, const_offset);

	Value const_value = chunk->constants.values[const_offset];
	print_value(const_value);

	printf("'\n");
	return offset + 2;
}

int const_long_instruction(const char * name, Chunk * chunk, int offset)
{
	// offset of the constant value in chunk->values
	uint32_t const_offset = 0;
	memcpy(&const_offset, &(chunk->bytecodes[offset + 1]), 3);
	printf("%-16s %4d '", name, const_offset);

	Value const_value = chunk->constants.values[const_offset];
	print_value(const_value);

	printf("'\n");
	return offset + 4;
}

int line_number(Chunk * chunk, int offset)
{
	uint16_t line;
	memcpy(&line, &(chunk->bytecodes[offset + 1]), 2);
	printf("%-16s %u\n", "LINE_NUMBER", line);
	printf("LINE %u:\n", line);
	return offset + 3;
}

int disassemble_inst(Chunk *chunk, int offset)
{
	printf("%04d ", offset);
	uint8_t inst = chunk->bytecodes[offset];	// Get the instruction
	switch (inst)
	{
		case OP_RETURN:
			return simple_instruction("OP_RETURN", offset);
		case OP_CONST:
			return const_instruction("OP_CONST", chunk, offset);
		case OP_CONST_LONG:
			return const_long_instruction("OP_CONST_LONG", chunk, offset);
		case OP_NEGATE:
			return simple_instruction("OP_NEGATE", offset);
		case OP_ADD:
			return simple_instruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simple_instruction("OP_SUBTRACT", offset);
		case OP_MUL:
			return simple_instruction("OP_MUL", offset);
		case OP_DIV:
			return simple_instruction("OP_DIV", offset);
		case META_LINE_NUM:
			return line_number(chunk, offset);
		default:
			printf("Unknown opcode\n");
			return offset + 1;
	}
	return offset;
}


// dump one bytecode line of the chunk, starting from 'start'
uint8_t chunk_bytecode_dump_line(Chunk * chunk, uint32_t start)
{
	uint8_t i;
	for (i = 0; i < 8 && start + i < chunk->size; i++)
	{
		printf("%02x ", chunk->bytecodes[start + i]);
	}
}

void chunk_bytecode_dump(Chunk * chunk, const char * name)
{
	printf("=== %s ===\n", name);
	uint32_t start = 0;
	while (start < chunk->size)
	{
		start += chunk_bytecode_dump_line(chunk, start);
		printf("\n");
	}
}
