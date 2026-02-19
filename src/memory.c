#include "memory.h"
#include <string.h>

#include "compiler.h"  // for mark_compiler_roots()
#include "object.h"
#include "vm.h"

#ifdef DBG_LOG_GC
#include <stdio.h>

#include "compiler.h"  // for mark_compiler_root()
#include "debug.h"
#endif

#ifdef DBG_LOG_GC
/** used to print object, the output format is specific for debugging purpose
 */
void dbg_print_object(Obj* obj) {
  printf("(\033[1;31m%p\033[0m, ", obj);
  print_value(OBJ_VAL(*obj));
  printf(")");
}
#endif

void* reallocate(void* arr, size_t old_sz, size_t new_sz) {
  vm.gc.allocated += (new_sz - old_sz);

  if (new_sz > old_sz) {
#ifdef DBG_STRESS_GC
    collect_garbage();
#else
    if (vm.gc.allocated >= vm.gc.threshold) {
      collect_garbage();
    }
#endif
  }

  if (new_sz == 0) {
    free(arr);
    return NULL;
  }

  void* new_arr = calloc(new_sz, sizeof(uint8_t));
  if (new_arr == NULL)
    exit(1);
  memcpy(new_arr, arr, old_sz);
  free(arr);

  return new_arr;
}

void free_string_obj(StringObj* obj) {
  FREE_ARRAY(char, obj->chars, obj->length + 1);
  FREE(StringObj, obj);
}

void free_function_obj(FunctionObj* obj) {
  chunk_free(&obj->chunk);
  FREE(FunctionObj, obj);
}

void free_closure_obj(ClosureObj* obj) {
  FREE(ClosureObj, obj);
}

void free_upvalue_obj(UpvalueObj* obj) {
  FREE(UpvalueObj, obj);
}

void free_native_fn_obj(NativeFnObj* obj) {
  FREE(NativeFnObj, obj);
}

void free_class_obj(ClassObj* obj) {
  table_free(&obj->methods);
  FREE(ClassObj, obj);
}

void free_instance_obj(InstanceObj* instance) {
  table_free(&(instance->fields));
  instance->klass = NULL;
  FREE(InstanceObj, instance);
}

void free_object(Obj* object) {
#ifdef DBG_LOG_GC
  printf("Free object at %p, object: ", (void*)object);
  print_value(OBJ_VAL(*object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_STRING:
      free_string_obj((StringObj*)object);
      break;
    case OBJ_FUNCTION:
      free_function_obj((FunctionObj*)object);
      break;
    case OBJ_CLOSURE:
      free_closure_obj((ClosureObj*)object);
      break;
    case OBJ_UPVALUE:
      free_upvalue_obj((UpvalueObj*)object);
      break;
    case OBJ_NATIVE_FN:
      free_native_fn_obj((NativeFnObj*)object);
      break;
    case OBJ_CLASS:
      free_class_obj((ClassObj*)object);
      break;
    case OBJ_INSTANCE:
      free_instance_obj((InstanceObj*)object);
      break;
    default:
      break;
  }
}

void free_objects() {
  Obj* current_obj = vm.objects;
  while (current_obj != NULL) {
    Obj* next = current_obj->next;
    free_object(current_obj);
    current_obj = next;
  }
}

/** Functions of the garbage collection module.
 */

bool mark_object(Obj* obj) {
  if (obj == NULL)
    return false;

#ifdef DBG_LOG_GC
  if (!obj->gc_marked) {
    printf("mark object: ");
    dbg_print_object(obj);
  } else {
    printf("reach a marked object: ");
    dbg_print_object(obj);
  }
  printf("\n");
#endif

  if (obj->gc_marked)
    return true;
  gc_push(obj);
  return (obj->gc_marked = true);
}

bool mark_value(Value val) {
  return IS_OBJ(val) && mark_object(AS_OBJ(val));
}

void mark_table(Table* table) {
  for (uint32_t i = 0; i < table->capacity; i++) {
    Entry* current = &(table->entries[i]);
    mark_object((Obj*)current->key);
    mark_value(current->value);
  }
}

/** mark_vm_roots: Mark all reachable roots
 *
 * The source of reachable nodes can be:
 * 	- In the VM's stack
 * 	- In the VM's frame (closure's upvalues)
 * */
void mark_vm_roots() {
  for (Value* it = vm.stack_top - 1; it >= vm.stack; it--) {
    mark_value(*it);
  }

#ifdef DBG_LOG_GC
  printf("Start discovering objects from global variable table\n");
#endif
  mark_table(&vm.globals);

#ifdef DBG_LOG_GC
  printf("Marked all objects reachable global variable table\n");
#endif

  mark_object(&vm.cls_init_strlit->obj);

#ifdef DBG_LOG_GC
  printf("Start discovering objects from call frames\n");
#endif

  for (CallFrame* frame = vm.frames; frame < &vm.frames[vm.frame_count];
       frame++) {
    mark_object((Obj*)frame->closure);
    for (int i = 0; i < frame->closure->upval_count; i++) {
      mark_object((Obj*)frame->closure->upvalues[i]);
    }
  }

#ifdef DBG_LOG_GC
  printf("Marked all objects reachable via call frames\n");
#endif

#ifdef DBG_LOG_GC
  printf("Start discovering objects reachable via vm.open_upvalues\n");
#endif

  for (UpvalueObj* upvalue = vm.open_upvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    mark_object((Obj*)upvalue);
  }

#ifdef DBG_LOG_GC
  printf("Marked discovering objects reachable via vm.open_upvalues\n");
#endif
}

/** Mark all reachable objects from object @obj.
 *
 *  @obj: the starting point to search for other reachable nodes.
 * */
void mark_reachable_objects(Obj* obj) {
#ifdef DBG_LOG_GC
        printf("Start discovering objects from: ");
        dbg_print_object(obj);
        printf("\n");
#endif
  switch (obj->type) {
    case OBJ_CLOSURE: {
      ClosureObj* closure = (ClosureObj*)obj;
      mark_object((Obj*)closure->function);
      for (int i = 0; i < closure->upval_count; i++) {
        if (closure->upvalues[i] == NULL) {
          continue;
        }

        mark_object((Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      FunctionObj* function = (FunctionObj*)obj;
      mark_object((Obj*)function->name);
      ValueArr* const_pool = &(function->chunk.constants);
      for (size_t i = 0; i < const_pool->size; i++) {
        mark_value(const_pool->values[i]);
      }
      break;
    }
    case OBJ_UPVALUE: {
      UpvalueObj* upvalue = (UpvalueObj*)obj;
      if (upvalue->value == NULL)
        mark_value(upvalue->cloned);
      else
        mark_value(*(upvalue->value));
      break;
    }
    case OBJ_CLASS: {
      mark_object((Obj*)obj);
      ClassObj* klass = (ClassObj*)obj;
      mark_table(&klass->methods);
      break;
    }
    case OBJ_INSTANCE: {
      InstanceObj* instance = (InstanceObj*)obj;
      mark_object((Obj*)instance->klass);
      mark_table(&(instance->fields));
      break;
    }
    case OBJ_BOUND_METHOD: {
      BoundMethodObj* bmethod = (BoundMethodObj*)obj;
      mark_value(bmethod->receiver);
      mark_object((Obj*)bmethod->method);
      break;
    }
    default:
      break;
  }
}

void discover_all_reachable() {
  while (!gc_empty()) {
    Obj* obj = gc_pop();
    mark_reachable_objects(obj);
#ifdef DBG_LOG_GC
    printf("Marked all objects reachable via: ");
    dbg_print_object(obj);
    printf("\n");
#endif
  }
}

void sweep_unreachable() {
  Obj* prev = NULL;
  Obj* current = vm.objects;

  while (current != NULL) {
    if (current->gc_marked) {
      // Reset the gc_marked state for the next garbage-collecting turn
      current->gc_marked = false;
      prev = current;
      current = current->next;
    } else {
      // The object is unreachable, delete it
      Obj* unreachable = current;
      current = current->next;
      if (prev == NULL)
        vm.objects = current;
      else
        prev->next = current;
      free_object(unreachable);
    }
  }
}

void collect_garbage() {
#ifdef DBG_LOG_GC
  printf("== begin gc ==\n");
#endif

#ifdef DBG_LOG_GC
  printf("gc mark_vm_roots\n");
#endif
  mark_vm_roots();
#ifdef DBG_LOG_GC
  printf("gc finishes mark_vm_roots\n");
#endif

#ifdef DBG_LOG_GC
  printf("gc mark_compiler_roots\n");
#endif
  mark_compiler_roots();
#ifdef DBG_LOG_GC
  printf("gc finishes mark_compiler_roots\n");
#endif

  discover_all_reachable();
  table_remove_unmarked_object(&vm.strings);
  sweep_unreachable();

#ifdef DBG_LOG_GC
  printf("== end gc ==\n");
#endif

  vm.gc.threshold = vm.gc.allocated * GC_GROW_FACTOR;
}
