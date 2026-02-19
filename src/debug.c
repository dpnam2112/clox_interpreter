#include "debug.h"
#include <stdio.h>
#include <string.h>
#include "chunk.h"
#include "object.h"
#include "value.h"

size_t current_line;
bool line_change;

int disassemble_inst(Chunk* chunk, size_t offset);

void disassemble_chunk(Chunk* chunk, const char* name) {
  // Initialize bytecode logger's state
  current_line = 0;
  line_change = false;

  printf("== %s ==\n", name);
  for (size_t offset = 0; offset < chunk->size;) {
    offset = disassemble_inst(chunk, offset);
  }
}

// simple_instruction: print out the instruction opcode and return
// the offset of the next one.
int simple_instruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int const_instruction(const char* name, Chunk* chunk, int offset) {
  // offset of the constant value in chunk->values
  uint8_t const_offset = chunk->bytecodes[offset + 1];
  printf("%-16s %4d ", name, const_offset);

  Value const_value = chunk->constants.values[const_offset];
  print_value(const_value);

  printf("\n");
  return offset + 2;
}

int single_param_inst(const char* name,
                      Chunk* chunk,
                      int offset,
                      size_t param_size) {
  // offset of the constant value in chunk->values
  size_t param = 0;
  memcpy(&param, &(chunk->bytecodes[offset + 1]), param_size);
  printf("%-16s %4zu", name, param);
  printf("\n");
  return offset + param_size + 1;
}

int call_instruction(const char* name, Chunk* chunk, int offset) {
  // offset of the constant value in chunk->values
  uint8_t const_offset = chunk->bytecodes[offset + 1];
  printf("%-16s %4d", name, const_offset);
  printf("\n");
  return offset + 2;
}

int invoke_instruction(const char* name, Chunk* chunk, int offset) {
  uint8_t const_offset = chunk->bytecodes[offset + 1];
  uint8_t param_count = chunk->bytecodes[offset + 2];
  printf("%-16s (%d args) %4d ", name, param_count, const_offset);
  print_value(chunk->constants.values[const_offset]);
  printf("\n");
  return offset + 3;
}

int invoke_long_instruction(const char* name, Chunk* chunk, int offset) {
  uint32_t const_offset = chunk->bytecodes[offset + 1];
  uint8_t param_count = chunk->bytecodes[offset + 4];
  printf("%-16s (%d args) %4d ", name, param_count, const_offset);
  print_value(chunk->constants.values[const_offset]);
  printf("\n");
  return offset + 5;
}

int const_long_instruction(const char* name, Chunk* chunk, int offset) {
  // offset of the constant value in chunk->values
  uint32_t const_offset = 0;
  memcpy(&const_offset, &(chunk->bytecodes[offset + 1]),
         LONG_CONST_OFFSET_SIZE);
  printf("%-16s %4d '", name, const_offset);

  Value const_value = chunk->constants.values[const_offset];
  print_value(const_value);

  printf("'\n");
  return offset + 4;
}

int upval_instruction(const char* name, Chunk* chunk, int offset) {
  Opcode opcode = chunk->bytecodes[offset];
  uint32_t upval_offset = 0;
  if (opcode == OP_GET_UPVAL_LONG || opcode == OP_SET_UPVAL_LONG) {
    memcpy(&upval_offset, &(chunk->bytecodes[offset + 1]),
           LONG_UPVAL_OFFSET_SIZE);
    printf("%-16s %4d\n", name, upval_offset);
    return offset + LONG_UPVAL_OFFSET_SIZE + 1;
  }

  upval_offset = chunk->bytecodes[offset + 1];
  printf("%-16s %4d\n", name, upval_offset);
  return offset + 2;
}

int jump_instruction(const char* name, Chunk* chunk, int offset) {
  uint16_t jmp_dist = *((uint16_t*)&(chunk->bytecodes[offset + 1]));
  uint32_t jmp_start = offset + 3;
  uint32_t jmp_dest = jmp_start + jmp_dist;
  printf("%-16s %4u -> %u\n", name, jmp_start, jmp_dest);
  return offset + 3;
}

int loop_instruction(const char* name, Chunk* chunk, int offset) {
  uint16_t jmp_dist = *((uint16_t*)&(chunk->bytecodes[offset + 1]));
  uint32_t jmp_start = offset + 3;
  uint32_t jmp_dest = jmp_start - jmp_dist;
  printf("%-16s %4u -> %u\n", name, jmp_start, jmp_dest);
  return offset + 3;
}

// int param_instruction(const char * name, Chunk * chunk, int offset, uint8_t
// param_size)
//{
//	Opcode opcode = chunk->bytecodes[offset];
//	uint32_t param = 0;
//	if (param_size == 1) param = chunk->bytecodes[offset + 1];
//	else memcpy(&param, &chunk->bytecodes[offset + 1], param_size);
//	printf("%-16s %-4d")
// }

int line_number(Chunk* chunk, int offset) {
  uint16_t line;
  memcpy(&line, &(chunk->bytecodes[offset + 1]), 2);
  printf("%-16s\n", "META_LINE_NUM");
  current_line = line;
  line_change = true;

  return offset + 3;
}

int disassemble_inst(Chunk* chunk, size_t offset) {
  printf("%04lu ", offset);
  Opcode inst = chunk->bytecodes[offset];  // Get the instruction
  size_t inst_line = chunk_get_line(chunk, offset);

  if (inst_line != current_line) {
    printf("%03lu ", inst_line);
    current_line = inst_line;
  } else
    printf("  | ");

  switch (inst) {
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
    case OP_TRUE:
      return simple_instruction("OP_TRUE", offset);
    case OP_FALSE:
      return simple_instruction("OP_FALSE", offset);
    case OP_NIL:
      return simple_instruction("OP_NIL", offset);
    case OP_NOT:
      return simple_instruction("OP_NOT", offset);
    case OP_EQUAL:
      return simple_instruction("OP_EQUAL", offset);
    case OP_LESS:
      return simple_instruction("OP_LESS", offset);
    case OP_GREATER:
      return simple_instruction("OP_GREATER", offset);
    case OP_PRINT:
      return simple_instruction("OP_PRINT", offset);
    case OP_POP:
      return simple_instruction("OP_POP", offset);
    case OP_DEFINE_GLOBAL:
      return const_instruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_GET_GLOBAL:
      return const_instruction("OP_GET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return const_instruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_LOCAL:
      return single_param_inst("OP_GET_LOCAL", chunk, offset, 1);
    case OP_SET_LOCAL:
      return single_param_inst("OP_SET_LOCAL", chunk, offset, 1);
    case OP_GET_UPVAL:
      return upval_instruction("OP_GET_UPVAL", chunk, offset);
    case OP_SET_UPVAL:
      return upval_instruction("OP_SET_UPVAL", chunk, offset);
    case OP_JMP:
      return jump_instruction("OP_JMP", chunk, offset);
    case OP_JMP_IF_FALSE:
      return jump_instruction("OP_JMP_IF_FALSE", chunk, offset);
    case OP_LOOP:
      return loop_instruction("OP_LOOP", chunk, offset);
    case OP_CALL:
      return call_instruction("OP_CALL", chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant_offset = chunk->bytecodes[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant_offset);
      print_value(chunk->constants.values[constant_offset]);
      printf("\n");

      ClosureObj* closure =
          AS_CLOSURE(chunk->constants.values[constant_offset]);
      for (int i = 0; i < closure->function->upval_count; i++) {
        uint8_t upval_info = chunk->bytecodes[offset++];
        bool local = upval_info & 1;
        bool long_offset = (upval_info & (1 << 1)) >> 1;

        uint32_t upval_index = 0;
        if (long_offset) {
          memcpy(&upval_index, &chunk->bytecodes[offset], 2);
          offset += 2;
        } else
          upval_index = chunk->bytecodes[offset++];

        printf("%04lu   |                     %s %d\n", offset - 2,
               local ? "local" : "upvalue", upval_index);
      }

      return offset;
    }
    case OP_CLOSURE_LONG:
      return const_long_instruction("OP_CLOSURE_LONG", chunk, offset);
    case OP_CLOSE_UPVAL:
      return simple_instruction("OP_CLOSE_UPVAL", offset);
    case OP_CLASS:
      return const_instruction("OP_CLASS", chunk, offset);
    case OP_CLASS_LONG:
      return const_long_instruction("OP_CLASS_LONG", chunk, offset);
    case OP_GET_PROPERTY:
      return const_instruction("OP_GET_PROPERTY", chunk, offset);
    case OP_GET_PROPERTY_LONG:
      return const_long_instruction("OP_GET_PROPERTY_LONG", chunk, offset);
    case OP_SET_PROPERTY:
      return const_instruction("OP_SET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY_LONG:
      return const_long_instruction("OP_SET_PROPERTY_LONG", chunk, offset);
    case OP_METHOD:
      return const_instruction("OP_METHOD", chunk, offset);
    case OP_METHOD_LONG:
      return const_long_instruction("OP_METHOD_LONG", chunk, offset);
    case OP_EXIT:
      return simple_instruction("OP_EXIT", offset);
    case OP_INVOKE:
      return invoke_instruction("OP_INVOKE", chunk, offset);
    case OP_INVOKE_LONG:
      return invoke_instruction("OP_INVOKE_LONG", chunk, offset);
    case OP_INHERIT:
      return simple_instruction("OP_INHERIT", offset);
    case OP_GET_SUPER:
      return const_instruction("OP_GET_SUPER", chunk, offset);
    case OP_GET_SUPER_LONG:
      return const_long_instruction("OP_GET_SUPER_LONG", chunk, offset);
    case OP_SUPER_INVOKE:
      return invoke_instruction("OP_SUPER_INVOKE", chunk, offset);
    case OP_SUPER_INVOKE_LONG:
      return invoke_instruction("OP_SUPER_INVOKE_LONG", chunk, offset);
    default:
      printf("Unknown opcode\n");
      return offset + 1;
  }

  return offset;
}
