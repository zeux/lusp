#pragma once

struct mem_arena_t;

bool lusp_memory_init(struct mem_arena_t* arena, size_t heap_size);
void lusp_memory_term();

void* lusp_memory_allocate(size_t size);
void lusp_memory_deallocate(void* ptr);

size_t lusp_memory_get_size();
