#pragma once

#include <stdbool.h>

struct mem_arena_t;

bool lusp_init(struct mem_arena_t* arena, unsigned int heap_size);
void lusp_term();
