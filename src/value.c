#include "value.h"
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"

bool value_equal(Value val_1, Value val_2) {
#ifdef NAN_BOXING
  if (IS_NUMBER(val_1) && IS_NUMBER(val_2)) {
    // A NaN **can** be a valid double. e.g. 0 / 0 == 0 / 0 -> this should return true.
    return AS_NUMBER(val_1) == AS_NUMBER(val_2);
  }

  if (IS_OBJ(val_1) && IS_OBJ(val_2)) {
    return object_equal(AS_OBJ(val_1), AS_OBJ(val_2));
  }

  return val_1 == val_2;
#else
  if (val_1.type != val_2.type)
    return false;

  switch (val_1.type) {
    case VAL_NIL:
      return true;
    case VAL_BOOL:
      return AS_BOOL(val_1) == AS_BOOL(val_2);
    case VAL_NUMBER:
      return AS_NUMBER(val_1) == AS_NUMBER(val_2);
    case VAL_OBJ: {
      return object_equal(AS_OBJ(val_1), AS_OBJ(val_2));
    }
    default:
      return false;
  }
#endif
}

void value_arr_init(ValueArr* vl_arr) {
  vl_arr->values = NULL;
  vl_arr->size = 0;
  vl_arr->capacity = 0;
}

void value_arr_free(ValueArr* vl_arr) {
  vl_arr->values = FREE_ARRAY(Value, vl_arr->values, vl_arr->capacity);
  value_arr_init(vl_arr);
}

void value_arr_append(ValueArr* vl_arr, Value val) {
  if (vl_arr->size == vl_arr->capacity) {
    uint32_t new_cap = GROW_CAPACITY(vl_arr->capacity);
    vl_arr->values =
        GROW_ARRAY(Value, vl_arr->values, vl_arr->capacity, new_cap);
    vl_arr->capacity = new_cap;
  }

  vl_arr->values[vl_arr->size++] = val;
}

void print_value(Value val) {
#ifdef NAN_BOXING
  if (IS_BOOL(val)) {
    printf(AS_BOOL(val) ? "true" : "false");
  } else if (IS_NUMBER(val)) {
    printf("%g", AS_NUMBER(val));
  } else if (IS_NIL(val)) {
    printf("nil");
  } else if (IS_OBJ(val)) {
    print_object(AS_OBJ(val));
  } else {
    printf("<?\?>");
  }
#else
  switch (val.type) {
    case VAL_BOOL:
      printf(AS_BOOL(val) ? "true" : "false");
      break;
    case VAL_NUMBER:
      printf("%g", AS_NUMBER(val));
      break;
    case VAL_NIL:
      printf("nil");
      break;
    case VAL_OBJ:
      print_object(AS_OBJ(val));
      break;
    default:
      printf("<?\?>");
  }
#endif
}

void print_object(Obj* obj) {
  switch (obj->type) {
    case OBJ_STRING:
      printf("'%s'", ((StringObj*) obj)->chars);
      break;
    case OBJ_FUNCTION: {
      FunctionObj* func = (FunctionObj*) obj;
      if (func->name == NULL)
        printf("<script>");
      else if (strcmp(func->name->chars, "") != 0)
        printf("<fn '%s'>", func->name->chars);
      else
        printf("<fn ?\?>");
      break;
    }
    case OBJ_UPVALUE:
      printf("<upvalue>");
      break;
    case OBJ_NATIVE_FN:
      printf("<native function>");
      break;
    case OBJ_CLOSURE: {
      ClosureObj* closure = (ClosureObj*) obj;
      StringObj* closure_name = closure->function->name;
      printf("<closure '%s'>",
             (closure_name == NULL) ? "" : closure_name->chars);
      break;
    }
    case OBJ_CLASS: {
      ClassObj* klass = (ClassObj*) obj;
      printf("<class '%s'>", klass->name->chars);
      break;
    }
    case OBJ_INSTANCE: {
      InstanceObj* instance = (InstanceObj*) obj;
      printf("<%s instance>", instance->klass->name->chars);
      break;
    }
    case OBJ_BOUND_METHOD: {
      BoundMethodObj* bmethod = (BoundMethodObj*) obj;
      StringObj* fun_name = bmethod->method->function->name;
      ClassObj* klass = AS_INSTANCE(bmethod->receiver)->klass;
      printf("<bound method '%s'.'%s'>", klass->name->chars, fun_name->chars);
      break;
    }
    case OBJ_NONE:
      printf("not an object");
      break;
  }
}

bool callable(Value val) {
  if (!IS_OBJ(val))
    return false;

  switch (AS_OBJ(val)->type) {
    case OBJ_FUNCTION:
    case OBJ_CLOSURE:
    case OBJ_NATIVE_FN:
    case OBJ_CLASS:
    case OBJ_BOUND_METHOD:
      return true;
    default:
      return false;
  }
}
