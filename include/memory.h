#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>

#include "value.h"

#define GC_GROW_FACTOR 2

#define GROW_ARRAY(type, ptr, old_sz, new_sz) \
  (type*)reallocate(ptr, sizeof(type) * old_sz, sizeof(type) * new_sz)

#define FREE_ARRAY(type, ptr, old_sz) \
  (type*)reallocate(ptr, sizeof(type) * old_sz, 0);

#define GROW_CAPACITY(old_capa) (old_capa < 32) ? 32 : old_capa * 2

#define BIT_WIDTH(type) sizeof(type) * 8

#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * count)
#define FREE(type, ptr) (type*)reallocate(ptr, sizeof(type), 0)

/* reallocate: Allocate a new array and move data from the old array
 * to the new one.
 * return value: pointer to the new array
 *
 * Special cases include:
 * old_sz = 0, new_sz = any (!= 0): allocate a new array
 * old_sz = any, new_sz = 0: free that array
 * */
void* reallocate(void* arr, size_t old_sz, size_t new_sz);
void* append_array(void* ptr,
                   size_t item_sz,
                   int arr_size,
                   int arr_capacity,
                   void* item);
void collect_garbage();
void free_objects();
bool mark_object(Obj* obj);

#endif
