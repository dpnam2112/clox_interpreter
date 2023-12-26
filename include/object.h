#ifndef OBJECT_H
#define OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

/** This header file contains all definitions of struct 'object' and its derivatives.
 *  
 * All object's derivative (Also called subclass in Object-oriented programming) contains
 * a field Obj obj, which indicates that it is derived from struct Obj.
*/

typedef enum
{
	OBJ_NONE,
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_NATIVE_FN,
} ObjType;

struct Obj
{
	ObjType type;
	Obj * next;

	// Used by garbage collector to indicate whether the object is marked
	bool gc_marked;
};

struct StringObj
{
	Obj obj;
	uint32_t length;
	char * chars;
	uint32_t hashcode;
};

typedef struct FunctionObj
{
	Obj obj;
	int arity;
	int upval_count;
	Chunk chunk;
	StringObj * name;
} FunctionObj;

typedef struct UpvalueObj
{
	Obj obj;
	Value * value;
	Value cloned;

	// Used by virtual machine to manage opening upvalues
	struct UpvalueObj * next;
} UpvalueObj;

typedef struct ClosureObj
{
	Obj obj;
	FunctionObj * function;
	UpvalueObj ** upvalues;
	int upval_count;
	int upval_capacity;
} ClosureObj;

typedef Value (*NativeFn) (int arg_count, Value * args);
typedef struct
{
	Obj obj;
	NativeFn function;
}
NativeFnObj;

#define OBJ_TYPE(obj) ((obj.type == VAL_OBJ) ? AS_OBJ(obj)->type : OBJ_NONE)

#define IS_STRING_OBJ(value) (is_obj_type(value, OBJ_STRING))
#define IS_FUNCTION_OBJ(value) (is_obj_type(value, OBJ_FUNCTION))
#define IS_CLOSURE_OBJ(value) (is_obj_type(value, OBJ_CLOSURE))
#define IS_UPVALUE_OBJ(value) (is_obj_type(value, OBJ_UPVALUE))
#define IS_NATIVE_FN_OBJ(value) (is_obj_type(value, OBJ_NATIVE_FN))

#define AS_STRING(value) ((StringObj*) AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)
#define AS_FUNCTION(value) ((FunctionObj *) AS_OBJ(value))
#define AS_CLOSURE(value) ((ClosureObj *) AS_OBJ(value))
#define AS_UPVALUE(value) ((UpvalueObj *) AS_OBJ(value))
#define AS_NATIVE_FN(value) (((NativeFnObj *) AS_OBJ(value))->function)

static inline bool is_obj_type(Value value, ObjType type)
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

/* StringObj_construct: Allocate a container object in heap memory
 * that contains a clone of the string pointed by @chars */
StringObj * StringObj_construct(const char * chars, size_t length);
FunctionObj * FunctionObj_construct();
ClosureObj * ClosureObj_construct();
UpvalueObj * UpvalueObj_construct();
NativeFnObj * NativeFnObj_construct(NativeFn func);

/* print_object: print the string representation of an object */
void print_object(Value);

uint32_t hash_string(const char *, int);

void * allocate_object(size_t size, ObjType type);
#endif
