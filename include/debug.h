#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"
#include "common.h"
#include <stdio.h>

void disassemble_chunk(Chunk * chunk, const char * name);
int disassemble_inst(Chunk * chunk, int offset);
void chunk_bytecode_dump(Chunk * chunk, const char * name);

#endif
