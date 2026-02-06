#include "chunk.h"
#include "test.h"

double add_ld_const_0() {
  // get size of chunk
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 3, 3);
  int byte_count = chunk.size;

  chunk_free(&chunk);
  return byte_count;
}

double add_ld_const_1() {
  // get size of chunk
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 3, 3);
  chunk_write_load_const(&chunk, 9, 6);

  int byte_count = chunk.size;

  chunk_free(&chunk);
  return byte_count;
}

double add_ld_const_2() {
  // get current line
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 3, 13);
  int current_line = chunk.current_line;

  chunk_free(&chunk);
  return current_line;
}

double add_ld_const_3() {
  // get current line
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 3, 13);
  chunk_write_load_const(&chunk, 23, 13);
  int current_line = chunk.current_line;

  chunk_free(&chunk);
  return current_line;
}

double add_ld_const_4() {
  // get current line
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 3, 13);
  chunk_write_load_const(&chunk, 56, 13);
  chunk_write_load_const(&chunk, 24, 3);
  chunk_write_load_const(&chunk, 33, 7);
  int current_line = chunk.current_line;

  chunk_free(&chunk);
  return current_line;
}

double add_ld_const_5() {
  // get chunk size
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 3, 13);   // 5
  chunk_write_load_const(&chunk, 56, 13);  // 7
  chunk_write_load_const(&chunk, 24, 3);   // 12
  chunk_write_load_const(&chunk, 33, 7);   // 17
  int sz = chunk.size;

  chunk_free(&chunk);
  return sz;
}

double add_ld_const_6() {
  // get chunk size
  Chunk chunk;
  chunk_init(&chunk);

  for (int i = 0; i < 256; i++) {
    chunk_write_load_const(&chunk, i, 1);
  }

  chunk_write_load_const(&chunk, 260, 2);
  int sz = chunk.size;
  chunk_free(&chunk);
  return sz;
}

double get_line_0() {
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 45, 3);
  int line = chunk_get_line(&chunk, 3);

  chunk_free(&chunk);
  return line;
}

double get_line_1() {
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 45, 3);
  chunk_write_load_const(&chunk, 23, 3);
  int line = chunk_get_line(&chunk, 4);

  chunk_free(&chunk);
  return line;
}

double get_line_2() {
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 45, 3);
  chunk_write_load_const(&chunk, 23, 5);
  int line = chunk_get_line(&chunk, 8);

  chunk_free(&chunk);
  return line;
}

double get_line_3() {
  Chunk chunk;
  chunk_init(&chunk);

  chunk_write_load_const(&chunk, 45, 3);
  chunk_write_load_const(&chunk, 23, 5);
  chunk_write_load_const(&chunk, 78, 5);
  int line = chunk_get_line(&chunk, 10);

  chunk_free(&chunk);
  return line;
}

void test_chunk() {
  printf("== testing chunk ==\n");

  test(add_ld_const_0, "Add a const-load instruction, line 3", 5);
  test(add_ld_const_1,
       "add two const-load instructions lying in two different lines", 10);
  test(add_ld_const_2, "Get current line", 13);
  test(add_ld_const_3, "Get current line, expect 13", 13);
  test(add_ld_const_4, "Get current line, expect 7", 7);
  test(add_ld_const_5, "Get current line, expect 17", 17);
  test(add_ld_const_6, "add 256 constants, get chunk size", 522);
  test(get_line_0, "Add a constant, line 3", 3);
  test(get_line_1, "Add two constants, line 3", 3);
  test(get_line_2, "Add three constants, two on the same line", 5);

  printf("== end of test ==\n");
}
