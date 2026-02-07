#include <stdio.h>
#include <stdlib.h>
#include "vm.h"

// read-eval-print loop
static void repl();

static void run_file(const char*);

/* Take file path as an argument and return string representation
 * of the source file. The returned buffer is allocated using malloc */
char* read_file(const char*);

/* Interpret the code */
InterpretResult interpret(const char*);

int main(int argc, char** argv) {
  vm_init(argc == 1);

  if (argc == 1) {
    // go to read-eval-print loop if the user pass no arguments
    repl();
  } else if (argc == 2) {
    // compile and execute the source program passed by the user
    run_file(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  vm_free();
}

static void repl() {
  char code[1024];
  while (true) {
    printf(">> ");
    if (!fgets(code, sizeof(code), stdin)) {
      printf("\n");
      break;
    }
    interpret(code);
  }
}

static void run_file(const char* path) {
  char* source = read_file(path);
  InterpretResult result = interpret(source);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  else if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

char* read_file(const char* path) {
  FILE* source_file = fopen(path, "r");
  if (source_file == NULL) {
    fprintf(stderr, "Cannot open '%s': File does not exist.\n", path);
    exit(74);
  }

  fseek(source_file, 0L, SEEK_END);
  size_t f_size = ftell(source_file);
  rewind(source_file);

  char* buffer = malloc(sizeof(char) * f_size + 1);

  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory.\n");
    exit(74);
  }

  // number of bytes read from the stream
  size_t read_count = fread(buffer, sizeof(char), f_size, source_file);
  if (read_count != f_size) {
    fprintf(stderr, "Cannot read the whole file.\n");
    exit(74);
  }

  buffer[f_size] = '\0';
  return buffer;
}
