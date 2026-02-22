#include <stdlib.h>
#include "object.h"
#define NAN_BOXING

#include <stdio.h>
#include "value.h"

int main() {
  Value nil = NIL_VAL();
  double num = val_to_num(nil);
  printf("%f\n", num);
  printf("nil is nil: %d\n", IS_NIL(nil));

  Value btrue = BOOL_VAL(true);
  num = val_to_num(btrue);
  printf("%f\n", num);
  printf("btrue is bool: %d\n", IS_BOOL(btrue));
  printf("check casting: %s\n", AS_BOOL(btrue) == true ? "OK" : "Failed");

  Value bfalse = BOOL_VAL(true);
  num = val_to_num(bfalse);
  printf("%f\n", num);
  printf("bfalse is bool: %d\n", IS_BOOL(bfalse));
  printf("check casting: %s\n", AS_BOOL(bfalse) == false ? "OK" : "Failed");

  Value realfl = num_to_val(0.123);
  num = val_to_num(realfl);
  printf("%f\n", num);
  printf("realfl is number: %d\n", IS_NUMBER(realfl));
  printf("realfl is bool: %d\n", IS_BOOL(realfl));
  printf("check casting: %s\n", AS_NUMBER(realfl) == 0.123 ? "OK" : "Failed");

  StringObj* str = malloc(sizeof(StringObj));
  num = val_to_num(OBJ_VAL(str));
  printf("%f\n", num);
  printf("is obj: %d\n", IS_OBJ(OBJ_VAL(str)));
  printf("is bool: %d\n", IS_BOOL(OBJ_VAL(str)));
  printf("check casting: %s\n", AS_OBJ(OBJ_VAL(str)) == &str->obj ? "OK" : "Failed");
}
