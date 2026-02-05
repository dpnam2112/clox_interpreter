#ifndef VALUE_H
#define VALUE_H

#include "common.h"

#define CONST_POOL_LIMIT  0x01000000 //2^24

typedef enum
{
    VAL_INTERR, // represent the internal errors
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ,
} ValueType;


typedef struct Obj Obj;
typedef struct StringObj StringObj;

typedef struct
{
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj * obj;
        int err_code;
	} as;
} Value;

bool value_equal(Value val_1, Value val_2);

/* Value initializers */
#define BOOL_VAL(value) (Value) { VAL_BOOL, {.boolean = value} }
#define NIL_VAL() (Value) { VAL_NIL, {.number = 0} }
#define NUMBER_VAL(value) (Value) { VAL_NUMBER, {.number = value} }
#define OBJ_VAL(object) (Value) { VAL_OBJ, {.obj = (Obj*) &object} }

/* Value cast */
#define AS_BOOL(value) ((Value) (value)).as.boolean
#define AS_NUMBER(value) ((Value) (value)).as.number
#define AS_OBJ(object) (((Value) (object)).as.obj)

/* type checkers */
#define IS_NUMBER(value) (((Value) (value)).type == VAL_NUMBER)
#define IS_NIL(value) (((Value) (value)).type == VAL_NIL)
#define IS_BOOL(value) (((Value) (value)).type == VAL_BOOL)
#define IS_OBJ(value) (((Value) (value)).type == VAL_OBJ)
#define IS_INTERR(value) (((Value) (value)).type == VAL_INTERR)

typedef struct
{
	Value * values;
	uint32_t size;
	uint32_t capacity;
} ValueArr;

void value_arr_init(ValueArr * vl_arr);
void value_arr_free(ValueArr * vl_arr);
void value_arr_append(ValueArr * vl_arr, Value val);
void print_value(Value val);
bool callable(Value val);

#endif
