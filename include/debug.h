#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>

#include "chunk.h"

void disassemble_chunk(Chunk* chunk, const char* name);

#endif
