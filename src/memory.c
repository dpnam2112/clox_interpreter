#include "memory.h"

void * reallocate(void * arr, size_t old_sz, size_t new_sz)
{
	if (new_sz == 0)
	{
		free(arr);
		return NULL;
	}

	void * new_arr = realloc(arr, new_sz);
	if (new_arr == NULL)
		exit(1);
	return new_arr;
}
