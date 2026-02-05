#include "table.h"
#include "memory.h"
#include "value.h"
#include "object.h"

void table_init(Table * table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void table_free(Table * table)
{
	FREE_ARRAY(Entry, table->entries, table->capacity);
	table_init(table);
}

/* find_entry: Find an empty entry or the entry contains the given key.
 *
 * return value: reference to the entry that either is empty or contains
 * the given key @key
 * @capacity: number of slots in @entries */
static Entry * find_entry(Entry * entries, uint32_t capacity, StringObj * key)
{
	Entry * tombstone = NULL;
	uint32_t start = key->hashcode & (capacity - 1), i = start, end_loop = false;
	while (!end_loop) {
		if (entries[i].key == NULL)
		{
			if (!entries[i].deleted)
				return (tombstone != NULL) ? tombstone : &entries[i];
			else if (tombstone == NULL)
				tombstone = &entries[i];
		}
		else if (entries[i].key == key)
			return &entries[i];
		i = (i + 1) % capacity;
		if (i == start) end_loop = true;
	}

	return tombstone;
}

static void table_expand(Table * table)
{
	uint32_t new_capacity = GROW_CAPACITY(table->capacity);
	Entry * expanded_entries = ALLOCATE(Entry, new_capacity);

	// Move all non-empty entries from the old table to the new one
	for (uint32_t i = 0; i < table->capacity; i++)
	{
		if (table->entries[i].key == NULL) continue;
		// Find an empty slot in the expanded table
		Entry * empty_slot = find_entry(expanded_entries, new_capacity,
						table->entries[i].key);
		empty_slot->key = table->entries[i].key;
		empty_slot->value = table->entries[i].value;
	}

	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = expanded_entries;
	table->capacity = new_capacity;
}

bool table_set(Table * table, StringObj * key, Value val)
{
	/* Ensure that the load factor does not exceed MAX_LOAD */
	if (table->count + 1 > MAX_LOAD * table->capacity)
		table_expand(table);
	Entry * entry = find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
        table->count++;
        entry->key = key;
        entry->value = val;
        return true;
    }

    return false;
}

bool table_get(Table * table, StringObj * key, Value * dest)
{
	if (table->capacity == 0)
		return false;
	Entry * target = find_entry(table->entries, table->capacity, key);
	if (target->key == NULL)
		return false;
	*dest = target->value;
	return true;
}

bool table_delete(Table * table, StringObj * key, Value * dest)
{
	if (table->capacity == 0)
		return false;
	Entry * target = find_entry(table->entries, table->capacity, key);
	if (target->key == NULL) 
		return false;
	if (dest != NULL)
		*dest = target->value;
	target->deleted = true;
	target->key = NULL;
//  table->count--;
	return true;
}

void table_remove_unmarked_object(Table * table)
{
	for (uint32_t i = 0; i < table->capacity; i++)
	{
		Entry * entry = &table->entries[i];
		if (entry->key != NULL && !entry->key->obj.gc_marked)
		{
			table_delete(table, entry->key, NULL);
		}
	}
}
