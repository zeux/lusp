// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/lusp.h>

#include <mem/arena.h>

struct mem_arena_t g_lusp_heap;

bool lusp_init(struct mem_arena_t* arena, unsigned int heap_size)
{
	return mem_arena_create_subarena(&g_lusp_heap, arena, heap_size, 16);
}

void lusp_term()
{
}

unsigned int lusp_heap_get_size()
{
	return g_lusp_heap.size - g_lusp_heap.free_size;
}
