// DeepLight Engine (c) Zeux 2006-2010

#pragma once

struct mem_arena_t;

bool lusp_init(struct mem_arena_t* arena, unsigned int heap_size);
void lusp_term();