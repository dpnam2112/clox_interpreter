#include "value.h"
#include "memory.h"

void value_arr_init(ValueArr * vl_arr)
{
	vl_arr->values = NULL;
	vl_arr->size = 0;
	vl_arr->capacity = 0;
}

void value_arr_free(ValueArr * vl_arr)
{
	vl_arr->values = FREE_ARRAY(Value, vl_arr->values, vl_arr->capacity);
	value_arr_init(vl_arr);
}

void value_arr_append(ValueArr * vl_arr, Value val)
{
	if (vl_arr->size == vl_arr->capacity)
	{
		uint32_t new_cap = GROW_CAPACITY(vl_arr->capacity);
		vl_arr->values = GROW_ARRAY(Value, vl_arr->values, vl_arr->capacity, new_cap);
		vl_arr->capacity = new_cap;
	}

	vl_arr->values[vl_arr->size++] = val;
}

void print_value(Value val)
{
	printf("%g", val);
}
