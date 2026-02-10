#ifndef TABLE_H
#define TABLE_H

#include "value.h"

#define MAX_LOAD 0.75

typedef struct StringObj StringObj;

typedef struct Entry {
  StringObj* key;
  bool tombstone;
  Value value;
} Entry;

typedef struct Table {
  uint32_t count;
  uint32_t capacity;
  Entry* entries;
} Table;

void table_init(Table* table);
void table_free(Table* table);

/* Replace value associated with a given key.
 * return a boolean value indicates whether
 * the key already exists in the table.
 * */
bool table_set(Table* table, StringObj* key, Value val);

/* get the value asscoiated to the given key
 * the retrieved value stored in the variable pointed by @dest
 * return true if the retrieval is successful */
bool table_get(Table* table, StringObj* key, Value* dest);

/* delete the entry whose key is @key and store the associated value
 * of the entry in the variable pointed by @dest
 * return true if the deletion is successful */
bool table_delete(Table* table, StringObj* key, Value* dest);

void table_remove_unmarked_object(Table* table);

#endif
