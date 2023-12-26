#ifndef VM_H
#define VM_H

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define STACK_MAX 		256
#define CALL_FRAME_MAX		64 

typedef struct
{
	ClosureObj * closure;
	uint8_t * pc;
	Value * slots;	// kind of a stack pointer
} CallFrame;

/** VM: object that belongs to this struct represent the virtual machine
 *
 * @frame: stack of frames that are called and are not terminate yet
 * @chunk: Contain the bytecodes and temporary constants
 * @pc: program counter, points to the next instruction that will be executed
 * @stack: contains values that are being used during the execution
 * @stack_top: points to the top of the stack, i.e, this pointer will point to the
 * next slot where the new value will be placed in the stack
 * @objects: Contains all dynamically allocated objects, later used by the garbage collector
 * @open_upvalues: used to manage upvalues (values that are used by functions to which the values
 * does not belong)
 */
typedef struct
{
	CallFrame frames[CALL_FRAME_MAX];
	uint8_t frame_count;
	Chunk * chunk;
	uint8_t * pc;	// program counter
	Value stack[STACK_MAX];
	Value * stack_top;
	Obj * objects;

	// All Upvalues in the list are sorted by the address of
	// the value they references to.
	// This means that the first upvalue of the list (the head) always 
	// references to the outermost upvalue inside the stack.
	// When the program counter escapes a scope, if there is any variables used by a outside function,
	// those variables need to be moved to the heap.
	// We use this linked list to track which upvalues need to be moved to the heap.
	UpvalueObj * open_upvalues;

	Table strings;	// used for string-interning technique
	Table globals; 	// global variables
	bool repl;

	// Used by garbage collector for mark-sweep garbage collecting algorithm
	// Contains all reachable heap objects that is marked, but is not discovered all of their children  
	struct {
		Obj ** objects;
		uint32_t count;
		uint32_t capacity;
	} gc_frontier;
} VM;

typedef enum
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void vm_init(bool is_repl);
void vm_free();
InterpretResult vm_interpret(Chunk * chunk);
void vm_stack_push(Value value);
Value vm_stack_pop();
int vm_stack_size();
bool gc_frontier_empty();
Obj * gc_frontier_pop();
void gc_frontier_push(Obj *);

#endif
