// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/memory.h>

#include <mem/arena.h>

static struct mem_arena_t g_lusp_arena;

struct mem_arena_t;

bool lusp_memory_init(struct mem_arena_t* arena, size_t heap_size)
{
	return mem_arena_create_subarena(&g_lusp_arena, arena, heap_size, 16);
}

void lusp_memory_term()
{
}

void* lusp_memory_allocate(size_t size)
{
    return mem_arena_allocate(&g_lusp_arena, size, 4);
}

void lusp_memory_deallocate(void* ptr)
{
    (void)ptr;
}

size_t lusp_memory_get_size()
{
    return g_lusp_arena.size - g_lusp_arena.free_size;
}