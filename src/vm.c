#include "vm.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chunk.h"
#include "compiler.h"
#include "memory.h"
#include "native_fns.h"
#include "object.h"
#include "table.h"
#include "value.h"

VM vm;

static void call_frame_reset() {
  // TODO: free the call frame resources
  vm.frame_count = 0;
}

static void stack_reset() {
  vm.stack_top = vm.stack;
}

inline void vm_stack_push(Value value) {
  *(vm.stack_top++) = value;
}

Value vm_stack_pop() {
  if (vm.stack_top == vm.stack) {
    printf("[memory error] pop empty stack.");
    exit(1);
  }

  return *(--vm.stack_top);
}

int vm_stack_size() {
  return vm.stack_top - vm.stack;
}

static Value vm_stack_peek(int distance) {
  return vm.stack_top[-1 - distance];
}

static Value clock_native(int param_count, Value* params) {
  assert(param_count >= 0);
  if (param_count == 0) {
    assert(params == NULL);
  }
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void define_native_fn(const char* name, NativeFn func) {
  vm_stack_push(OBJ_VAL(*StringObj_construct(name, strlen(name))));
  vm_stack_push(OBJ_VAL(*NativeFnObj_construct(func)));
  table_set(&vm.globals, AS_STRING(vm_stack_peek(1)), vm_stack_peek(0));
  vm_stack_pop();
  vm_stack_pop();
}

void vm_init(bool repl) {
  stack_reset();
  call_frame_reset();
  table_init(&vm.strings);
  table_init(&vm.globals);

  vm.open_upvalues = NULL;
  vm.objects = NULL;
  vm.frame_count = 0;
  vm.repl = repl;
  vm.gc.objects = NULL;
  vm.gc.count = 0;
  vm.gc.capacity = 0;
  vm.gc.allocated = 0;
  vm.gc.threshold = 2 << 8;

  vm.cls_init_strlit = NULL;
  vm.cls_init_strlit = StringObj_construct("init", 4);

  define_native_fn("clock", clock_native);
  define_native_fn("hasattr", native_fn_has_attribute);
}

void vm_free() {
  table_free(&vm.strings);
  table_free(&vm.globals);
  free_objects();
  stack_reset();
  call_frame_reset();
  vm.cls_init_strlit = NULL;
}

bool gc_empty() {
  return vm.gc.count == 0;
}

void gc_push(Obj* obj) {
  if (vm.gc.count == vm.gc.capacity) {
    int new_cap = GROW_CAPACITY(vm.gc.capacity);
    vm.gc.objects = realloc(vm.gc.objects, new_cap * sizeof(Obj*));
    vm.gc.capacity = new_cap;
  }

  vm.gc.objects[vm.gc.count++] = obj;
}

Obj* gc_pop() {
  if (vm.gc.count == 0)
    return NULL;
  return vm.gc.objects[--vm.gc.count];
}

static bool is_falsey(Value val) {
  return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

static inline uint16_t read_short(CallFrame* frame) {
  uint16_t v = *((uint16_t*)frame->pc);
  frame->pc += 2;
  return v;
}

/** Concatenate two strings that are on top of the stack.
 *  The result of the concatenation is pushed back into the stack.
 * */
static Value concatenate(Value left, Value right) {
  StringObj* sleft = AS_STRING(left);
  StringObj* sright = AS_STRING(right);

  size_t total_length = sright->length + sleft->length;

  char* concat_string = ALLOCATE(char, total_length + 1);
  memcpy(concat_string, sleft->chars, sleft->length);
  memcpy(concat_string + sleft->length, sright->chars, sright->length);
  concat_string[total_length] = '\0';

  StringObj* concat_str_obj = StringObj_construct(concat_string, total_length);
  FREE(char, concat_string);
  return OBJ_VAL(*concat_str_obj);
}

/* runtime_error: print out error message to stderr and reset the stack */
static void runtime_error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // print stack trace in the following format:
  // line [i1], in a1()
  // line [i2], in a2()
  // ...
  // line [iN], in script
  for (CallFrame* frame = &vm.frames[vm.frame_count - 1]; frame >= vm.frames;
       frame--) {
    FunctionObj* function = frame->closure->function;
    int inst_offset = frame->pc - function->chunk.bytecodes;
    int line = chunk_get_line(&function->chunk, inst_offset);
    fprintf(stderr, "[line %d] in ", line);
    if (function->name == NULL)
      fprintf(stderr, "script\n");
    else
      fprintf(stderr, "%s()\n", function->name->chars);
  }

  //	CallFrame * frame = &vm.frames[vm.frame_count - 1];
  //	size_t inst_offset = frame->pc - frame->function->chunk.bytecodes - 1;
  //	uint16_t line = chunk_get_line(frame->function->chunk, inst_offset);
  //	fprintf(stderr, "[line %d] in script\n", line);
  stack_reset();
  call_frame_reset();
}

/** panic: for internal runtime errors that cannot be recovered.
 * stop the runtime immediately.
 * TODO: implement placeholder for graceful shutdown.
 * */
static void panic(const char* format, ...) {
  va_list args;
  fprintf(stderr, "\033[1;31m [panic] \033[0m");
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  exit(-1);
}

/** read n bytes from a chunk
 * @param byte_count: number of bytes which represents the offset
 * @param pc: pointer pointing to the program counter
 *
 * @param byte_count does not exceed 4. If it does, it will be changed to 4.
 * return value: positive integer value
 */
static uint32_t read_bytes(uint8_t** pc, uint8_t byte_count) {
  // TODO: declare its prototype as read_short's
  if (byte_count > 4)
    byte_count = 4;
  uint32_t bytes = 0;
  memcpy(&bytes, *pc, byte_count);
  *pc += byte_count;
  return bytes;
}

/** push_call_frame: push a new call frame on top of the call stack.
 * Return the new call frame.
 */
static CallFrame* vm_call_frame_push(ClosureObj* closure, int param_count) {
  if (param_count != closure->function->arity) {
    runtime_error("Expect %d parameters but got %d.", closure->function->arity,
                  param_count);
    return NULL;
  }

  CallFrame* new_frame = &vm.frames[vm.frame_count++];
  new_frame->closure = closure;
  new_frame->pc = closure->function->chunk.bytecodes;
  new_frame->slots = vm.stack_top - param_count - 1;
  return new_frame;
}

static bool call_value(Value value, int param_count) {
  if (!callable(value)) {
    return false;
  }

  // Check if the frame stack is overflow
  if (vm.frame_count >= CALL_FRAME_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  if (IS_CLOSURE_OBJ(value)) {
    ClosureObj* closure = AS_CLOSURE(value);
    CallFrame* frame = vm_call_frame_push(closure, param_count);
    if (frame == NULL) {
      return false;
    }
  } else if (IS_CLASS_OBJ(value)) {
    ClassObj* class_obj = AS_CLASS(value);
    InstanceObj* new_instance = InstanceObj_construct(class_obj);
    Value v_init;

    if (table_get(&class_obj->methods, vm.cls_init_strlit, &v_init)) {
      if (!IS_CLOSURE_OBJ(v_init)) {
        panic("initalizer is not a closure.");
      }

      ClosureObj* init = AS_CLOSURE(v_init);
      CallFrame* frame = vm_call_frame_push(init, param_count);
      frame->slots[0] = OBJ_VAL(*new_instance);
    } else {
      vm.stack_top[-1] = OBJ_VAL(*new_instance);
    }
  } else if (IS_NATIVE_FN_OBJ(value)) {
    NativeFn native_fn = AS_NATIVE_FN(value);
    Value res;
    if (param_count > 0) {
      res = native_fn(param_count, vm.stack_top - param_count);
    } else {
      res = native_fn(0, NULL);
    }
    vm.stack_top -= param_count + 1;
    vm_stack_push(res);
  } else if (IS_BOUND_METHOD_OBJ(value)) {
    BoundMethodObj* bmethod = AS_BOUND_METHOD(value);
    ClosureObj* method = bmethod->method;
    CallFrame* new_frame = vm_call_frame_push(method, param_count);
    if (!new_frame) {
      return false;
    }

    new_frame->slots[0] = bmethod->receiver;
  }

  return true;
}

/** @capture_upval: Create an upvalue object that references to @value
 * @param value: value that the new upvalue will reference to
 * @return the new upvalue object.
 *
 * NOTE: New upvalues will be added to the virtual machine's list of upvalues.
 * When the virtual machine escape a scope, it will move all upvalues inside
 * the scope to the heap space (see close_upval() for more detail).
 */
static UpvalueObj* capture_upval(Value* value) {
  // Iterate through the linked list of upvalues until
  // An upvalue object that references to @value is found

  UpvalueObj *prev = NULL, *current = vm.open_upvalues;
  while (current != NULL && value < current->value) {
    prev = current;
    current = current->next;
  }

  if (current != NULL && value == current->value) {
    return current;
  }

  UpvalueObj* new_upvalue = UpvalueObj_construct(value);
  if (prev != NULL) {
    new_upvalue->next = prev->next;
    prev->next = new_upvalue;
  } else {
    vm.open_upvalues = new_upvalue;
  }

  return new_upvalue;
}

/** @close_upvalues: Move all values referenced by upvalue objects to the heap
 * @param last: the last upvalues to be closed
 *
 * NOTE: All upvalues in the virtual machine's list of upvalues is sorted by
 * comparing Upvalue.value, which is a pointer pointing to a value inside the
 * stack.
 *
 * When an upvalue is closed, we clone the value to which the upvalue reference
 * and put the new value inside the upvalue object.
 */
static void close_upvalues(Value* last) {
  while (vm.open_upvalues != NULL && vm.open_upvalues->value >= last) {
    UpvalueObj* closed_upval = vm.open_upvalues;
    closed_upval->cloned = *(closed_upval->value);
    closed_upval->value = &closed_upval->cloned;
    vm.open_upvalues = closed_upval->next;
    closed_upval->next = NULL;
  }
}

static InterpretResult run() {
  // current frame being executed

#define READ_BYTE() *(frame->pc++)
#define READ_SHORT() (read_short(frame))
#define READ_BYTES(n) read_bytes(&(frame->pc), n)
#define READ_CONST() \
  frame->closure->function->chunk.constants.values[READ_BYTE()]
#define READ_CONST_LONG()                   \
  frame->closure->function->chunk.constants \
      .values[READ_BYTES(LONG_CONST_OFFSET_SIZE)]
#define READ_CONST_AT(offset) \
  frame->closure->function->chunk.constants.values[offset]
#define BINARY_OP(value_type, op)                                        \
  do {                                                                   \
    if (!(IS_NUMBER(vm_stack_peek(0)) && IS_NUMBER(vm_stack_peek(1)))) { \
      runtime_error("Operands must be numbers.");                        \
      return INTERPRET_RUNTIME_ERROR;                                    \
    }                                                                    \
    double right = AS_NUMBER(vm_stack_pop());                            \
    double left = AS_NUMBER(vm_stack_pop());                             \
    vm_stack_push(value_type(left op right));                            \
  } while (false)

  for (;;) {
    CallFrame* frame = &vm.frames[vm.frame_count - 1];
#ifdef DBG_TRACE_EXECUTION
    /* Print stack values */
    printf("== begin value stack trace ==\n");
    for (Value* ptr = vm.stack; ptr < vm.stack_top; ++ptr) {
      printf("[ ");
      print_value(*ptr);
      printf(" ]\n");
    }
    printf("== end value stack trace ==\n");
#endif
    Opcode inst;
    switch (inst = READ_BYTE()) {
      case OP_EXIT:
        return INTERPRET_OK;
      case OP_RETURN: {
        Value return_value = vm_stack_pop();
        if (vm.frame_count == 1) {
          vm.frame_count = 0;
          vm_stack_pop();  // pop the top-level function
          return INTERPRET_OK;
        }

        close_upvalues(frame->slots);
        vm.stack_top = frame->slots;
        vm.frame_count--;
        frame = &vm.frames[vm.frame_count - 1];
        vm_stack_push(return_value);
        break;
      }
      case OP_CONST:
      case OP_CONST_LONG: {
        Value constant = (inst == OP_CONST) ? READ_CONST() : READ_CONST_LONG();
        vm_stack_push(constant);
        break;
      }
      case OP_TRUE: {
        vm_stack_push(BOOL_VAL(true));
        break;
      }
      case OP_FALSE: {
        vm_stack_push(BOOL_VAL(false));
        break;
      }
      case OP_NIL: {
        vm_stack_push(NIL_VAL());
        break;
      }
      case OP_NEGATE: {
        if (vm_stack_peek(0).type != VAL_NUMBER) {
          runtime_error("Cannot negate an object that is not numeric");
          return INTERPRET_RUNTIME_ERROR;
        }

        Value val_wrapper = vm_stack_pop();
        double negated_val = -AS_NUMBER(val_wrapper);
        vm_stack_push(NUMBER_VAL(negated_val));
        break;
      }
      case OP_NOT: {
        Value val_wrapper = vm_stack_pop();
        vm_stack_push(BOOL_VAL(is_falsey(val_wrapper)));
        break;
      }
      case OP_ADD: {
        Value left = vm_stack_peek(1);
        Value right = vm_stack_peek(0);
        Value result;

        bool are_nums = right.type == VAL_NUMBER && left.type == VAL_NUMBER;
        bool are_strs =
            OBJ_TYPE(right) == OBJ_STRING && OBJ_TYPE(left) == OBJ_STRING;

        if (!(are_nums || are_strs)) {
          runtime_error("Both operands must be either strings or numbers");
          return INTERPRET_RUNTIME_ERROR;
        }

        if (right.type == VAL_NUMBER) {
          result = NUMBER_VAL(AS_NUMBER(left) + AS_NUMBER(right));
        } else {
          result = concatenate(left, right);
        }

        vm_stack_pop();
        vm_stack_pop();
        vm_stack_push(result);
        break;
      }
      case OP_SUBTRACT:
        BINARY_OP(NUMBER_VAL, -);
        break;
      case OP_MUL:
        BINARY_OP(NUMBER_VAL, *);
        break;
      case OP_DIV:
        BINARY_OP(NUMBER_VAL, /);
        break;
      case OP_EQUAL: {
        Value x = vm_stack_pop();
        Value y = vm_stack_pop();
        vm_stack_push(BOOL_VAL(value_equal(x, y)));
        break;
      }
      case OP_LESS:
        BINARY_OP(BOOL_VAL, <);
        break;
      case OP_GREATER:
        BINARY_OP(BOOL_VAL, >);
        break;
      case OP_PRINT: {
        Value value = vm_stack_pop();
        print_value(value);
        printf("\n");
        break;
      }
      case OP_POP:
        vm_stack_pop();
        break;
      case OP_DEFINE_GLOBAL:
      case OP_DEFINE_GLOBAL_LONG: {
        uint32_t iden_offset = (inst == OP_DEFINE_GLOBAL)
                                   ? READ_BYTE()
                                   : READ_BYTES(LONG_CONST_OFFSET_SIZE);
        StringObj* identifier = AS_STRING(READ_CONST_AT(iden_offset));
        table_set(&vm.globals, identifier, vm_stack_peek(0));
        vm_stack_pop();
        break;
      }
      case OP_GET_GLOBAL:
      case OP_GET_GLOBAL_LONG: {
        uint32_t offset = (inst == OP_GET_GLOBAL)
                              ? READ_BYTE()
                              : READ_BYTES(LONG_CONST_OFFSET_SIZE);
        StringObj* identifier = AS_STRING(READ_CONST_AT(offset));
        Value value;
        if (!table_get(&vm.globals, identifier, &value)) {
          runtime_error("Undefined identifier: '%s'.", identifier->chars);
          return INTERPRET_RUNTIME_ERROR;
        } else {
          vm_stack_push(value);
        }
        break;
      }
      case OP_SET_GLOBAL:
      case OP_SET_GLOBAL_LONG: {
        uint32_t offset = (inst == OP_SET_GLOBAL)
                              ? READ_BYTE()
                              : READ_BYTES(LONG_CONST_OFFSET_SIZE);
        StringObj* identifier = AS_STRING(READ_CONST_AT(offset));
        Value rhs = vm_stack_peek(0);
        if (!table_set(&vm.globals, identifier, rhs)) {
          // the identifier has yet to be defined.
          runtime_error("Undefined identifier: '%s'.", identifier->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_LOCAL:
      case OP_GET_LOCAL_LONG: {
        uint32_t slot = (inst == OP_GET_LOCAL)
                            ? READ_BYTE()
                            : READ_BYTES(LONG_LOCAL_OFFSET_SIZE);
        vm_stack_push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL:
      case OP_SET_LOCAL_LONG: {
        uint32_t slot = (inst == OP_SET_LOCAL)
                            ? READ_BYTE()
                            : READ_BYTES(LONG_LOCAL_OFFSET_SIZE);
        frame->slots[slot] = vm_stack_peek(0);
        break;
      }
      case OP_JMP_IF_FALSE: {
        uint16_t jmp_dist = READ_SHORT();
        if (is_falsey(vm_stack_peek(0)))
          frame->pc += jmp_dist;
        break;
      }
      case OP_JMP: {
        uint16_t jmp_dist = READ_SHORT();
        frame->pc += jmp_dist;
        break;
      }
      case OP_LOOP: {
        uint32_t jmp_dist = READ_SHORT();
        frame->pc -= jmp_dist;
        break;
      }
      case OP_CALL: {
        uint8_t param_count = READ_BYTE();
        Value called_obj = vm_stack_peek(param_count);

        if (!callable(called_obj)) {
          runtime_error("object is not callable.");
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!call_value(called_obj, param_count)) {
          runtime_error("failed to call function.");
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_CLOSURE:
      case OP_CLOSURE_LONG: {
        /** Load the closure object from the constant pool and
         * capture all upvalues that are refered during the closure
         * execution.
         * */
        Value closure_val =
            (inst == OP_CLOSURE) ? READ_CONST() : READ_CONST_LONG();
        vm_stack_push(closure_val);
        ClosureObj* closure = AS_CLOSURE(closure_val);

        for (int i = 0; i < closure->function->upval_count; i++) {
          uint8_t upval_info = READ_BYTE();

          bool local = upval_info & 1;
          bool long_offset = (upval_info & (1 << 1)) >> 1;

          uint32_t upvalue_pos = (long_offset) ? READ_BYTES(2) : READ_BYTE();

          if (local) {
            closure->upvalues[i] = capture_upval(frame->slots + upvalue_pos);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[upvalue_pos];
          }
        }

        break;
      }
      case OP_GET_UPVAL:
      case OP_GET_UPVAL_LONG: {
        uint32_t upval_index = (inst == OP_GET_UPVAL)
                                   ? READ_BYTE()
                                   : READ_BYTES(LONG_UPVAL_OFFSET_SIZE);
        UpvalueObj* upvalue = frame->closure->upvalues[upval_index];
        vm_stack_push(*(upvalue->value));
        break;
      }
      case OP_SET_UPVAL:
      case OP_SET_UPVAL_LONG: {
        uint32_t upval_index = (inst == OP_SET_UPVAL)
                                   ? READ_BYTE()
                                   : READ_BYTES(LONG_UPVAL_OFFSET_SIZE);
        UpvalueObj* upvalue = frame->closure->upvalues[upval_index];
        *(upvalue->value) = vm_stack_peek(0);
        break;
      }
      case OP_CLOSE_UPVAL: {
        close_upvalues(vm.stack_top - 1);
        vm_stack_pop();
        break;
      }
      case OP_CLASS:
      case OP_CLASS_LONG: {
        // Create a new class, take class name's offset in the chunk's
        // array of values as parameter.
        Value class_name_val =
            (inst == OP_CLASS) ? READ_CONST() : READ_CONST_LONG();
        ClassObj* new_class = ClassObj_construct(AS_STRING(class_name_val));
        vm_stack_push(OBJ_VAL(*new_class));
        break;
      }
      case OP_GET_PROPERTY:
      case OP_GET_PROPERTY_LONG: {
        /** Get a property from the top object in the stack. After the
         * operation is completed, the instance will be popped from the
         * stack and the property value of the instance will be pushed into
         * the stack.
         *
         * In the case the resolved identity is not a field but a method (which
         * is of type closure), the operation returns a bound method object.
         *
         * ============= Bytecode Format =============
         * OP_GET_PROPERTY     	<1-byte offset>
         * OP_GET_PROPERTY_LONG <3-byte offset>
         * ===========================================
         * <offset>: Offset of the string representing the property's name
         * in the value array.
         * */
        Value name =
            (inst == OP_GET_PROPERTY) ? READ_CONST() : READ_CONST_LONG();
        Value top = vm_stack_peek(0);
        if (!IS_INSTANCE_OBJ(top)) {
          runtime_error("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        InstanceObj* instance = AS_INSTANCE(vm_stack_peek(0));
        Value property, method;

        // try property look-up first, then method look-up
        if (table_get(&instance->fields, AS_STRING(name), &property)) {
          vm_stack_pop();  // Pop the class instance.
          vm_stack_push(property);
        } else if (table_get(&instance->klass->methods, AS_STRING(name),
                             &method)) {
          if (!IS_CLOSURE_OBJ(method)) {
            panic("method must be a closure.");
          }

          BoundMethodObj* bmethod =
              BoundMethodObj_construct(top, AS_CLOSURE(method));
          vm_stack_pop();
          vm_stack_push(OBJ_VAL(*bmethod));
        } else {
          StringObj* class_name = instance->klass->name;
          runtime_error("'%s' object has no property '%s'.", class_name->chars,
                        AS_CSTRING(name));
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SET_PROPERTY:
      case OP_SET_PROPERTY_LONG: {
        /** Stack's precondition:
         * == Stack ==
         * ... <class instance> <value>
         * ===========
         *
         * Stack's postcondition:
         * == Stack ==
         * ...
         * ===========
         *
         * Set the property of the class instance to value <value>.
         * == Bytecode format ==
         * OP_SET_PROPERTY <offset>
         * =====================
         * @param <offset> the offset of the StringObj representing the
         * property's name in the value array.
         * */
        Value property_name_val =
            (inst == OP_SET_PROPERTY) ? READ_CONST() : READ_CONST_LONG();

        if (!IS_INSTANCE_OBJ(vm_stack_peek(1))) {
          runtime_error("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        Value rhs_value = vm_stack_peek(0);
        InstanceObj* instance = AS_INSTANCE(vm_stack_peek(1));
        table_set(&(instance->fields), AS_STRING(property_name_val), rhs_value);
        rhs_value = vm_stack_pop();
        vm_stack_pop();
        vm_stack_push(rhs_value);
        break;
      }
      case OP_METHOD:
      case OP_METHOD_LONG: {
        /** OP_METHOD/OP_METHOD_LONG adds the closure at the top of the
         * value stack to the method table of the class object next to
         * that closure. It takes the method name's offset in the constant
         * pool as the argument.
         *
         * Value stack's pre-condition:
         * == Stack ==
         * ... <class> <closure>
         * ===========
         *
         * Value stack's post-condition:
         * == Stack ==
         * ... <class>
         * ===========
         * */

        Value method_name_val =
            (inst == OP_METHOD) ? READ_CONST() : READ_CONST_LONG();
        if (!(IS_CLASS_OBJ(vm_stack_peek(1)) &&
              IS_CLOSURE_OBJ(vm_stack_peek(0)) &&
              IS_STRING_OBJ(method_name_val))) {
          panic("OP_METHOD's pre-condition check fails.");
        }

        StringObj* method_name = AS_STRING(method_name_val);
        ClassObj* klass = AS_CLASS(vm_stack_peek(1));
        ClosureObj* method = AS_CLOSURE(vm_stack_peek(0));

        bool exist =
            table_set(&klass->methods, method_name, OBJ_VAL(method->obj));
        if (exist) {
          // deleting the class from global variable table,
          // if it's defined globally
          table_delete(&vm.globals, klass->name, NULL);
          runtime_error("Duplicate method name ('%s') in class '%s'.",
                        method_name->chars, klass->name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        vm_stack_pop();
        break;
      }
      case OP_INVOKE: {
        Value v_method_name = READ_CONST();
        uint8_t param_count = READ_BYTE();

        if (!(IS_STRING_OBJ(v_method_name))) {
          panic("(OP_INVOKE) expect a string argument and a number argument.");
        }

        StringObj* method_name = AS_STRING(v_method_name);

        Value v_instance = vm_stack_peek(param_count);
        if (!IS_INSTANCE_OBJ(v_instance)) {
          runtime_error("The receiver is not an instance.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ClassObj* klass = AS_INSTANCE(v_instance)->klass;
        Value callable_val;

        // check if there is any property defined with the name
        if (table_get(&AS_INSTANCE(v_instance)->fields, method_name,
                      &callable_val)) {
          if (!callable(callable_val)) {
            runtime_error("property is not callable.");
            return INTERPRET_RUNTIME_ERROR;
          }
        } else {
          // resolve method
          if (!table_get(&klass->methods, method_name, &callable_val)) {
            runtime_error("Class '%s' doesn't have method/property '%s'.",
                          klass->name->chars, AS_CSTRING(v_method_name));
            return INTERPRET_RUNTIME_ERROR;
          }

          if (!IS_CLOSURE_OBJ(callable_val)) {
            panic("'method' must be a closure.");
          }
        }

        if (!call_value(callable_val, param_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_BYTES
#undef READ_SHORT
#undef READ_CONST
#undef READ_CONST_LONG
#undef READ_CONST_AT
#undef BINARY_OP
}
InterpretResult vm_interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.pc = chunk->bytecodes;
  return run();
}

InterpretResult interpret(const char* source) {
  ClosureObj* closure = compile(source);

#ifdef DEBUG_LOG
  printf("compiled the source code");
#endif

  if (closure == NULL)
    return INTERPRET_COMPILE_ERROR;

  vm_stack_push(OBJ_VAL(*closure));
  // push the top-level code to the frame stack
  call_value(OBJ_VAL(*closure), 0);
  return run();
}
