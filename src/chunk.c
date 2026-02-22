#include "chunk.h"
#include <string.h>

#include <assert.h>
#include <complex.h>
#include <stdint.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// Expand the chunk's bytecode array so that it can contain more n bytes
#define FIT_EXPAND_CHUNK(chunk, n)                                         \
  do {                                                                     \
    while (chunk->size + n > chunk->capacity) {                            \
      uint32_t new_cap = GROW_CAPACITY(chunk->capacity);                   \
      if (new_cap > CHUNK_CONST_POOL_MAX) {                                \
        new_cap = CHUNK_CONST_POOL_MAX;                                    \
      }                                                                    \
      chunk->bytecodes =                                                   \
          GROW_ARRAY(uint8_t, chunk->bytecodes, chunk->capacity, new_cap); \
      chunk->capacity = new_cap;                                           \
    }                                                                      \
  } while (false)

void chunk_init(Chunk* chunk) {
  chunk->size = 0;
  chunk->capacity = 0;
  chunk->bytecodes = NULL;
  chunk->current_line = 0;
  chunk->line_tracker = NULL;
  value_arr_init(&(chunk->constants));
}

void chunk_free(Chunk* chunk) {
  if (chunk->bytecodes != NULL) {
    FREE_ARRAY(uint8_t, chunk->bytecodes, chunk->capacity);
  }

  value_arr_free(&(chunk->constants));
  chunk_init(chunk);

  while (chunk->line_tracker != NULL) {
    BytecodeLine* deleted = chunk->line_tracker;
    chunk->line_tracker = chunk->line_tracker->next;
    free(deleted);
  }
}

/* Add information to track line numbers of bytecodes
 * @bytecode_pos: position of the bytecode
 * @line: line number associated to the bytecode at @bytecode_pos
 */
void add_line_metadata(Chunk* chunk, uint32_t bytecode_pos, uint32_t line) {
  if (line == chunk->current_line && chunk->line_tracker != NULL) {
    return;
  }

  BytecodeLine* bytecode_line = malloc(sizeof(BytecodeLine));
  bytecode_line->line = line;
  bytecode_line->pos = bytecode_pos;

  bytecode_line->next = chunk->line_tracker;
  chunk->line_tracker = bytecode_line;
}

void chunk_append(Chunk* chunk, uint8_t byte, uint16_t line) {
  FIT_EXPAND_CHUNK(chunk, 1);
  chunk->bytecodes[chunk->size] = byte;
  add_line_metadata(chunk, chunk->size, line);
  chunk->size++;
}

int get_inst_size(Opcode type) {
  switch (type) {
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

uint16_t chunk_get_line(Chunk* chunk, uint32_t index) {
  BytecodeLine* iter = chunk->line_tracker;
  for (; iter != NULL; iter = iter->next) {
    if (index >= iter->pos)
      break;
  }

  return iter->line;
}

void chunk_append_bytes(Chunk* chunk, void* bytes, int n) {
  FIT_EXPAND_CHUNK(chunk, n);
  memcpy(&(chunk->bytecodes[chunk->size]), bytes, n);
  chunk->size += n;
}

uint32_t chunk_add_const(Chunk* chunk, Value value) {
  if (chunk->constants.size + 1 > CHUNK_CONST_POOL_MAX)
    return CHUNK_CONST_POOL_EFULL;

  // The call of value_arr_append can trigger the garbage collector,
  // and the @value which will be pushed maybe used later.
  // So we temporarily push @value into the vm's stack to prevent @value
  // from being removed by the gc.

  vm_stack_push(value);
  value_arr_append(&chunk->constants, value);
  vm_stack_pop();
  return chunk->constants.size - 1;
}

void chunk_get_const(Chunk* chunk, uint32_t offset, Value* dest) {
  assert(offset < chunk->constants.size);
  *dest = chunk->constants.values[offset];
}

void chunk_write_load_const(Chunk* chunk, Value value, uint16_t line) {
  // position of the value in constant pool
  uint32_t const_offset = 0;
  const_offset = chunk_add_const(chunk, value);

  Opcode load_const, load_const_long;

  if (IS_CLOSURE_OBJ(value)) {
    load_const = OP_CLOSURE, load_const_long = OP_CLOSURE_LONG;
  } else {
    load_const = OP_CONST, load_const_long = OP_CONST_LONG;
  }

  // add instruction's opcode
  chunk_append(
      chunk, (const_offset <= UINT8_MAX) ? load_const : load_const_long, line);
  chunk_append_bytes(chunk, &const_offset,
                     (const_offset <= UINT8_MAX) ? 1 : LONG_CONST_OFFSET_SIZE);
}

bool chunk_const_pool_is_full(Chunk* chunk) {
  return (chunk->constants.size >= CHUNK_CONST_POOL_MAX);
}
