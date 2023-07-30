#ifndef OBJECT_H
#define OBJECT_H

#include "common.h"
#include "value.h"

typedef enum
{
	OBJ_NONE,
	OBJ_STRING,
} ObjType;

struct Obj
{
	ObjType type;
	Obj * next;
};

struct StringObj
{
	Obj obj;
	uint32_t length;
	char * chars;
	uint32_t hashcode;
};

#define OBJ_TYPE(obj) ((obj.type == VAL_OBJ) ? AS_OBJ(obj)->type : OBJ_NONE)
#define IS_STRING_OBJ(value) (is_obj_type(value, OBJ_STRING))
#define AS_STRING(value) ((StringObj*) AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)

static inline bool is_obj_type(Value value, ObjType type)
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

/* StringObj_construct: Allocate a container object in heap memory
 * that contains a clone of the string pointed by @chars */
StringObj * StringObj_construct(const char * chars, size_t length);

/* print_object: print the string representation of an object */
void print_object(Value);

uint32_t hash_string(const char *, int);

void * allocate_object(size_t size, ObjType type);
#endif
