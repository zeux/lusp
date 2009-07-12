// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct mem_arena_t;

bool lusp_init(struct mem_arena_t* arena, unsigned int heap_size);
void lusp_term();

unsigned int lusp_heap_get_size();
