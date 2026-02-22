#include "object.h"
#include <string.h>
#include "memory.h"
#include "table.h"
#include "vm.h"

#define OBJ_ALLOC(type, objectType) \
  (type*)allocate_object(sizeof(type), objectType)

void* allocate_object(size_t size, ObjType type) {
  Obj* obj_ref = reallocate(NULL, 0, size);
  obj_ref->type = type;
  obj_ref->gc_marked = false;

#ifdef DBG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)obj_ref, size, type);
#endif

  /* add the new object to virtual machine's object pool */
  obj_ref->next = vm.objects;
  vm.objects = obj_ref;

  return obj_ref;
}

/* hash_string: hash a string using FNV algorithm */
uint32_t hash_string(const char* s, int length) {
  uint32_t hashcode = 2166136261u;       // FNV offset basis
  const uint32_t fnv_prime = 16777619u;  // FNV prime
  for (int i = 0; i < length; i++) {
    hashcode ^= s[i];
    hashcode *= fnv_prime;
  }
  return hashcode;
}

/* return the entry containing a string that is the same as @chars or
 * an empty entry. */
Entry* find_existing_string_entry(const char* chars,
                                  size_t length,
                                  uint32_t hashcode) {
  if (vm.strings.capacity == 0)
    return NULL;
  uint32_t start = hashcode % vm.strings.capacity;
  Entry* entry = NULL;
  for (uint32_t i = 0; i < vm.strings.capacity; i++) {
    uint32_t entry_idx = (start + i) % vm.strings.capacity;
    entry = &vm.strings.entries[entry_idx];
    if (entry->key != NULL) {
      StringObj* current_str = entry->key;
      if (current_str->length == length &&
          memcmp(current_str->chars, chars, length) == 0) {
        return entry;
      }
    } else if (!entry->tombstone) {
      return entry;
    }
  }

  return NULL;
}

StringObj* StringObj_allocate(char* chars, size_t length, uint32_t hashcode) {
  StringObj* str_obj = OBJ_ALLOC(StringObj, OBJ_STRING);
  str_obj->chars = chars;
  str_obj->length = length;
  str_obj->hashcode = hashcode;
  return str_obj;
}

StringObj* StringObj_construct(const char* chars, size_t length) {
  /* If there is an existing string that is the same as @chars,
   * we reuse that string.
   * If there isn't, @entry is the empty entry where we will put the
   * string object in. */
  uint32_t hashcode = hash_string(chars, length);
  Entry* entry = find_existing_string_entry(chars, length, hashcode);
  if (entry != NULL && entry->key != NULL)
    return entry->key;

  /* Clone the string, the new string is stored in heap */
  char* str_clone = ALLOCATE(char, length + 1);
  memcpy(str_clone, chars, length);
  str_clone[length] = '\0';

  /* Allocate a string object (StringObj) to store the clone string */
  StringObj* str_obj = StringObj_allocate(str_clone, length, hashcode);

  vm_stack_push(OBJ_VAL(*str_obj));

  /* Add the new string to the string table */
  if (entry == NULL) {
    table_set(&vm.strings, str_obj, NIL_VAL());
  } else {
    entry->key = str_obj;
    entry->value = NIL_VAL();
    entry->tombstone = false;
    vm.strings.count++;
  }

  vm_stack_pop();

  return str_obj;
}

FunctionObj* FunctionObj_construct() {
  FunctionObj* function = OBJ_ALLOC(FunctionObj, OBJ_FUNCTION);
  function->arity = 0;
  function->upval_count = 0;
  chunk_init(&function->chunk);
  function->name = NULL;
  return function;
}

ClosureObj* ClosureObj_construct(FunctionObj* function) {
  ClosureObj* closure = OBJ_ALLOC(ClosureObj, OBJ_CLOSURE);
  // allocation for UpvalueObj may trigger the garbage collection process
  vm_stack_push(OBJ_VAL(*closure));

  closure->function = function;
  closure->upvalues =
      (function->upval_count)
          ? ALLOCATE(UpvalueObj*, sizeof(UpvalueObj*) * function->upval_count)
          : NULL;
  for (int i = 0; i < function->upval_count; i++) {
    closure->upvalues[i] = NULL;
  }
  closure->upval_count = function->upval_count;
  vm_stack_pop();
  return closure;
}

UpvalueObj* UpvalueObj_construct(Value* value) {
  UpvalueObj* upvalue = OBJ_ALLOC(UpvalueObj, OBJ_UPVALUE);
  upvalue->value = value;
  upvalue->next = NULL;
  return upvalue;
}

NativeFnObj* NativeFnObj_construct(NativeFn func) {
  NativeFnObj* new_native = OBJ_ALLOC(NativeFnObj, OBJ_NATIVE_FN);
  new_native->function = func;
  return new_native;
}

ClassObj* ClassObj_construct(StringObj* name) {
  ClassObj* new_class = OBJ_ALLOC(ClassObj, OBJ_CLASS);
  new_class->name = name;
  table_init(&new_class->methods);
  return new_class;
}

InstanceObj* InstanceObj_construct(ClassObj* klass) {
  InstanceObj* new_instance = OBJ_ALLOC(InstanceObj, OBJ_INSTANCE);
  new_instance->klass = klass;
  table_init(&(new_instance->fields));
  return new_instance;
}

BoundMethodObj* BoundMethodObj_construct(Value receiver, ClosureObj* method) {
  BoundMethodObj* bmethod = OBJ_ALLOC(BoundMethodObj, OBJ_BOUND_METHOD);
  bmethod->receiver = receiver;
  bmethod->method = method;
  return bmethod;
}

bool object_equal(Obj* obj1, Obj* obj2) {
  if (obj1->type == OBJ_STRING && obj2->type == OBJ_STRING) {
    StringObj* str_1 = (StringObj*) obj1;
    StringObj* str_2 = (StringObj*) obj2;
    return str_1->length == str_2->length &&
           strcmp(str_1->chars, str_2->chars) == 0;
  }

  return obj1 == obj2;
}
