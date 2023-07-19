#ifndef VM_H
#define VM_H

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"

#define STACK_MAX 256

typedef struct
{
	Chunk * chunk;
	uint8_t * pc;	// program counter
	Value stack[STACK_MAX];
	Value * stack_top;
} VM;

typedef enum
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm_init();
void vm_free();
InterpretResult vm_interpret(Chunk * chunk);
void vm_stack_push(Value value);
Value vm_stack_pop();
int vm_stack_size();

#endif