#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "value.h"

#define MAX_LOAD 0.75

typedef struct Entry
{
	StringObj * key;
	bool deleted;	// was the entry deleted before?
	Value value;
} Entry;

typedef struct Table
{
	uint32_t count;
	uint32_t capacity;
	Entry * entries;
} Table;

void table_init(Table * table);
void table_free(Table * table);

/* 'assign' value to the given key @key
 * return true if the given key is not associated to a value before. */
bool table_set(Table * table, StringObj * key, Value val);

/* get the value asscoiated to the given key
 * the retrieved value stored in the variable pointed by @dest
 * return true if the retrieval is successful */
bool table_get(Table * table, StringObj * key, Value * dest);

/* delete the entry whose key is @key and store the associated value
 * of the entry in the variable pointed by @dest
 * return true if the deletion is successful */
bool table_delete(Table * table, StringObj * key, Value * dest);

void table_remove_unmarked_object(Table * table);

#endif
