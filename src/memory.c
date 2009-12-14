// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/memory.h>

#include <mem/arena.h>
#include <mem/heap.h>

static struct mem_heap_t g_lusp_heap;

bool lusp_memory_init(struct mem_arena_t* arena, size_t heap_size)
{
	void* memory = mem_arena_allocate(arena, heap_size, 16);
	
	mem_heap_create_on_buffer(&g_lusp_heap, memory, heap_size, 0, "lusp");
	
	return true;
}

void lusp_memory_term()
{
}

void* lusp_memory_allocate(size_t size)
{
    return mem_heap_allocate(&g_lusp_heap, size, 4);
}

void lusp_memory_deallocate(void* ptr)
{
    mem_heap_deallocate(&g_lusp_heap, ptr);
}

size_t lusp_memory_get_size()
{
    return g_lusp_heap.size - g_lusp_heap.free_size;
}