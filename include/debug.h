#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>

#include "chunk.h"

void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_inst(Chunk* chunk, size_t offset);
void chunk_bytecode_dump(Chunk* chunk, const char* name);

#endif
