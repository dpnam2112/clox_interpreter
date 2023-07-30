#include "memory.h"
#include "object.h"
#include "vm.h"

void * reallocate(void * arr, size_t old_sz, size_t new_sz)
{
	if (new_sz == 0)
	{
		free(arr);
		return NULL;
	}

	void * new_arr = calloc(new_sz, sizeof(uint8_t));
	if (new_arr == NULL)
		exit(1);
	memcpy(new_arr, arr, old_sz);
	free(arr);
	
	return new_arr;
}

void free_string_obj(StringObj * obj)
{
	FREE_ARRAY(char, obj->chars, obj->length + 1);
	FREE(StringObj, obj);
}

void free_object(Obj * object)
{
	switch (object->type)
	{
		case OBJ_STRING:
			free_string_obj(object);
			break;
	}
}

void free_objects()
{
	Obj * current_obj = vm.objects;
	while (current_obj != NULL)
	{
		Obj * next = current_obj->next;
		free_object(current_obj);
		current_obj = next;
	}
}
