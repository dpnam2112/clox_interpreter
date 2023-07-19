#ifndef VALUE_H
#define VALUE_H

#include "common.h"
#include "memory.h"

typedef double Value;
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

#endif
