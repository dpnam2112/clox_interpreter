#include "native_fns.h"

#include <assert.h>

#include "object.h"
#include "value.h"

bool _has_attribute(Value value, StringObj* attrname) {
  if (!IS_INSTANCE_OBJ(value)) {
    return false;
  }

  InstanceObj* instance = AS_INSTANCE(value);
  Value dest;
  return table_get(&(instance->fields), attrname, &dest);
}

Value native_fn_has_attribute(int param_count, Value* params) {
  assert(param_count == 2);
  Value val = params[0];
  StringObj* attrname = AS_STRING(params[1]);
  return BOOL_VAL(_has_attribute(val, attrname));
}
