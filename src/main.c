#include "common.h"
#include "debug.h"
#include "chunk.h"
#include "vm.h"

int main(int argc, char ** argv)
{
	Chunk chunk;
	chunk_init(&chunk);

	for (Value i = 0; i < 258; i++)
	{
		chunk_write_load_const(&chunk, i, 1);
	}

	disassemble_chunk(&chunk, "Disassemble chunk");

	chunk_free(&chunk);
}
