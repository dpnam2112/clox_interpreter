#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct Obj Obj;

#ifdef NAN_BOXING
#include <string.h>

#define BITMASK_EQ(value, pattern) ((value & (pattern)) == (pattern))

/* NaNs, according to IEEE-754, are representations that
 * CPUs must not treat as doubles/floats. A representation
 * is a NaN if all 11 exponent bits are set to 1.
 *
 * When encountering a NaN, a CPU either:
 * - Triggers a trap/exception if the quiet bit is set, or,
 * - Justs do nothing with it (aborting).
 *
 * NaNs with the quiet bit set are called 'quiet NaNs', and
 * those that have the quiet bit unset are called 'signal NaNs'.
 *
 * We utilize NaN to represent Values as a more space-efficient
 * format: instead of using struct, now we pack Value's fields into
 * the unused bits of quiet NaNs (those that are not in the 11
 * exponent bits and the quiet bit).
 */
#define IEEE754_NAN_MASK (UINT64_C(0x7ff) << 52)

/* Quiet bit of IEEE-754 floating-point.
 *
 * CPUs should not raise any faults/traps when it encounters any
 * NaN having this bit set.
 */
#define IEEE754_QBIT (UINT64_C(1) << 51)

/* IEEE-754's sign bit indicates whether a float is negative.
 *
 * But here we use it to represent Obj pointer. If this bit
 * is set, we should treat Value as an object.
 */
#define IEEE754_SIGN_BIT (UINT64_C(1) << 63)
#define INTEL_FP_INDEF (UINT64_C(1) << 50)
#define QNAN (IEEE754_NAN_MASK | IEEE754_QBIT | INTEL_FP_INDEF)

/* Use the second bit to distinguish boolean and nil */
#define TAG_NIL 0x01
#define TAG_BOOL 0x10
#define TAG_OBJ IEEE754_SIGN_BIT

typedef uint64_t Value;

/* Use type punning technique to convert number to value and vice versa */

static inline Value num_to_val(double num) {
  Value val;
  memcpy(&val, &num, sizeof(double));
  return val;
}

static inline double val_to_num(Value val) {
  double num;
  memcpy(&num, &val, sizeof(Value));
  return num;
}

/* Value cast */
#define AS_BOOL(value) \
  (BITMASK_EQ(value, QNAN | TAG_BOOL | true) ? true : false)
#define AS_NUMBER(value) val_to_num(value)
#define AS_OBJ(value) ((Obj*)(((uintptr_t)value) & ~(TAG_OBJ | QNAN)))

/* Value initializers */
#define NIL_VAL() ((Value)(QNAN | TAG_NIL))
#define BOOL_VAL(b) (QNAN | TAG_BOOL | (b ? true : false))
#define NUMBER_VAL(num) num_to_val(num)
#define OBJ_VAL(object) (Value)((TAG_OBJ | QNAN | (uintptr_t) & object))

/* type checkers */
// not a quiet NaN -> value should be treated as a normal double.
#define IS_NUMBER(value) (!BITMASK_EQ(value, QNAN))
#define IS_NIL(value) (value == NIL_VAL())
#define IS_BOOL(value) \
  ((!BITMASK_EQ(value, TAG_OBJ)) && BITMASK_EQ(value, TAG_BOOL | QNAN))
#define IS_OBJ(value) BITMASK_EQ(value, TAG_OBJ | QNAN)

#else

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as;
} Value;

/* Value initializers */
#define BOOL_VAL(value) \
  (Value) {             \
    VAL_BOOL, {         \
      .boolean = value  \
    }                   \
  }
#define NIL_VAL() \
  (Value) {       \
    VAL_NIL, {    \
      .number = 0 \
    }             \
  }
#define NUMBER_VAL(value) \
  (Value) {               \
    VAL_NUMBER, {         \
      .number = value     \
    }                     \
  }
#define OBJ_VAL(object)    \
  (Value) {                \
    VAL_OBJ, {             \
      .obj = (Obj*)&object \
    }                      \
  }

/* Value cast */
#define AS_BOOL(value) ((Value)(value)).as.boolean
#define AS_NUMBER(value) ((Value)(value)).as.number
#define AS_OBJ(object) (((Value)(object)).as.obj)

/* type checkers */
#define IS_NUMBER(value) (((Value)(value)).type == VAL_NUMBER)
#define IS_NIL(value) (((Value)(value)).type == VAL_NIL)
#define IS_BOOL(value) (((Value)(value)).type == VAL_BOOL)
#define IS_OBJ(value) (((Value)(value)).type == VAL_OBJ)

#endif  // #if NAN_BOXING

/* Limits */
#define MAX_UPVALUE 256
#define MAX_LOCALVAR 256
#define MAX_CALLARGS UINT8_MAX

typedef struct {
  Value* values;
  uint32_t size;
  uint32_t capacity;
} ValueArr;

bool value_equal(Value val_1, Value val_2);

void value_arr_init(ValueArr* vl_arr);
void value_arr_free(ValueArr* vl_arr);
void value_arr_append(ValueArr* vl_arr, Value val);
void print_value(Value val);
bool callable(Value val);

#endif
