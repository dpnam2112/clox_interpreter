#include "value.h"
#include "object.h"
#include "memory.h"

bool value_equal(Value val_1, Value val_2)
{
	if (val_1.type != val_2.type)
		return false;

	switch (val_1.type)
	{
		case VAL_NIL: return true;
		case VAL_BOOL: return AS_BOOL(val_1) == AS_BOOL(val_2);
		case VAL_NUMBER: return AS_NUMBER(val_1) == AS_NUMBER(val_2);
		case VAL_OBJ:
			{
				StringObj * str_1 = AS_STRING(val_1);
				StringObj * str_2 = AS_STRING(val_2);
				return str_1->length == str_2->length &&
					strcmp(str_1->chars, str_2->chars) == 0;
			}
		default: return false;
	}
}

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
	switch (val.type) {
		case VAL_BOOL:	printf((AS_BOOL(val)) ? "true" : "false"); break;
		case VAL_NIL: 	printf("nil"); break;
		case VAL_OBJ:	print_object(val); break;
		default: 	printf("%g", AS_NUMBER(val));
	}
}

void print_object(Value val)
{
	switch (OBJ_TYPE(val))
	{
		case OBJ_STRING:
			printf ("'%s'", AS_CSTRING(val));
			break;
		case OBJ_FUNCTION:
		{
			FunctionObj * func = AS_FUNCTION(val);
			if (!func->name)
				printf("<fn>");
			else if (strcmp(func->name->chars, "") != 0)
				printf ("<fn '%s'>", func->name->chars);
			else
				printf("<script>");
			break;
		}
		case OBJ_UPVALUE:
			printf("<upvalue>");
			break;
		case OBJ_NATIVE_FN:
			printf("<native function>");
			break;
		case OBJ_CLOSURE:
			ClosureObj* closure = AS_CLOSURE(val);
			StringObj* closure_name = closure->function->name;
			printf("<closure '%s'>", (closure_name == NULL) ? "" : closure_name->chars);
			break;
		case OBJ_CLASS:
			printf("<class '%s'>", AS_CLASS(val)->name->chars);
			break;
		case OBJ_INSTANCE:
			printf("<%s instance>",
					AS_INSTANCE(val)->klass->name->chars);
			break;
		case OBJ_NONE:
			printf("not an object");
			break;
	}
}

bool callable(Value val)
{
	if (val.type != VAL_OBJ)
		return false;

	switch (OBJ_TYPE(val))
	{
		case OBJ_FUNCTION:
		case OBJ_CLOSURE:
		case OBJ_NATIVE_FN:
		case OBJ_CLASS:
			return true;
		default:
			return false;
	}
}
