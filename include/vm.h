#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256
#define CALL_FRAME_MAX 64

typedef struct {
  ClosureObj* closure;
  uint8_t* pc;
  Value* slots;  // kind of a stack pointer
} CallFrame;

/** VM: object that belongs to this struct represent the virtual machine
 *
 * @frame: stack of frames that are called and are not terminate yet
 * @chunk: Contain the bytecodes and temporary constants
 * @pc: program counter, points to the next instruction that will be executed
 * @stack: contains values that are being used during the execution
 * @stack_top: points to the top of the stack, i.e, this pointer will point to
 * the next slot where the new value will be placed in the stack
 * @objects: Contains all dynamically allocated objects, later used by the
 * garbage collector
 * @open_upvalues: used to manage upvalues (values that are used by functions
 * to which the values does not belong)
 */
typedef struct {
  CallFrame frames[CALL_FRAME_MAX];
  uint8_t frame_count;
  Chunk* chunk;
  uint8_t* pc;  // program counter
  Value stack[STACK_MAX];
  Value* stack_top;
  Obj* objects;

  /** Upvalues all scoped variables refered by closures.
   *
   * Initially, these upvalues only refer to these variables by pointers
   * (Value* value).
   *
   * When the VM exits a scope, these local variables will be destroyed.
   * To keep them alive, they will be 'closed' -- put inside their
   * respective upvalues -- after the scope ends.
   *
   * The list of upvalues are sorted by the addresses of local variables they
   * refer to, in ascending order. For example: if the scope looks like
   * this:
   *
   * {
   *    var a = 1;
   *    var c = 3;
   *    var b = 2;
   *
   *    fun getB() {
   *      return b;
   *    }
   *
   *    fun getA() {
   *      return a;
   *    }
   * }
   *
   * Then, the value stack when this scope is executed looks like this:
   * value stack: [a] [b] [getB] [getA]
   *
   * In this case, only 'a' and 'b' are refered inside functions. Then, there
   * are two upvalues refering two them and they are ordered by the addresses
   * of their counterpart in the value stack:
   *
   * Open upvalues: [Upvalue (referring to a)] [Upvalue (referring to b)]
   *
   * When the scope exits, OP_CLOSE_UPVAL is executed and the upvalues
   * hold values of the local variables they refer to:
   *
   * Closure's upvalue array: [Upvalue holding a] [Upvalue holding b]
   *
   * When the closures are invoked outside the destroyed scope,
   * They use the captured values which put in their upvalue array.
   * */
  UpvalueObj* open_upvalues;

  Table strings;  // used for string-interning technique
  Table globals;  // global variables
  bool repl;

  /** @gc stores the data used by the garbage collection algorithm.
   * The garbage collector is implemented using mark-sweep algorithm.
   *
   * The idea is to use a graph traversal algorithm (in this case,
   * Breadth-first traversal) to mark all reachable objects. After the graph
   * traversal completes, we remove all unreachable objects from @vm.objects.
   *
   * @gc.objects: A dynamic array used to keep tracks of objects that are
   * reachable and are not marked.
   * @gc.count: Number of objects in @gc.objects
   * @gc.capacity: Maximum capacity of @gc.objects
   *
   * Two fields @gc.allocated and @gc.threshold are used to determine when
   * to collect garbages. If @gc.allocated > @gc.threshold, the garbage
   * collection operation is performed.
   * */
  struct {
    Obj** objects;
    uint32_t count;
    uint32_t capacity;
    size_t allocated;
    size_t threshold;
  } gc;

  // The method name of class initializers. In this case, it's
  // literally equivalent to 'init'.
  StringObj* cls_init_strlit;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void vm_init(bool is_repl);
void vm_free();

InterpretResult vm_interpret(Chunk* chunk);

void vm_stack_push(Value value);
Value vm_stack_pop();
int vm_stack_size();

/** gc_empty: check if the BFS array used by gc is empty.  */
bool gc_empty();

/** gc_pop: Remove an object from the BFS array used by gc. */
Obj* gc_pop();

/** gc_push: Add a new object to the BFS array. */
void gc_push(Obj*);

#endif
