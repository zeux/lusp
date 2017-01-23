#include "memory.h"

#include <stdlib.h>

bool lusp_memory_init(struct mem_arena_t* arena, size_t heap_size)
{
	(void)arena;
	(void)heap_size;

	return true;
}

void lusp_memory_term()
{
}

void* lusp_memory_allocate(size_t size)
{
    return malloc(size);
}

void lusp_memory_deallocate(void* ptr)
{
    free(ptr);
}

size_t lusp_memory_get_size()
{
    return 0;
}
