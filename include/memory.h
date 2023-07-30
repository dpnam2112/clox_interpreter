#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include <stdlib.h>

#define GROW_ARRAY(type, ptr, old_sz, new_sz) \
	(type *) reallocate(ptr, sizeof(type) * old_sz, sizeof(type) * new_sz)

#define FREE_ARRAY(type, ptr, old_sz) \
	(type *) reallocate(ptr, sizeof(type) * old_sz, 0);

#define GROW_CAPACITY(old_capa) \
	(old_capa < 8) ? 8 : old_capa * 2

#define BIT_WIDTH(type) sizeof(type) * 8

#define ALLOCATE(type, size) (type *) reallocate(NULL, 0, sizeof(type) * size)
#define FREE(type, ptr) (type *) reallocate(ptr, sizeof(type), 0)

/* reallocate: Allocate a new array and move data from the old array
 * to the new one.
 * return value: pointer to the new array
 *
 * Special cases include:
 * old_sz = 0, new_sz = any (!= 0): allocate a new array
 * old_sz = any, new_sz = 0: free that array
 * */
void * reallocate(void * arr, size_t old_sz, size_t new_sz);
void free_objects();

#endif
