// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/lusp.h>

#include <lusp/object.h>

#include <mem/arena.h>

struct mem_arena_t g_lusp_heap;

bool lusp_init(struct mem_arena_t* arena, unsigned int heap_size)
{
	// create heap
	if (!mem_arena_create_subarena(&g_lusp_heap, arena, heap_size, 16)) return false;
	
	// initialize builtin objects
	if (!lusp_internal_object_init()) return false;
	
	return true;
}

void lusp_term()
{
	lusp_internal_object_term();
}

unsigned int lusp_heap_get_size()
{
	return g_lusp_heap.size - g_lusp_heap.free_size;
}
