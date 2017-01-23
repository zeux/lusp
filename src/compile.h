#pragma once

#include "object.h"

struct lusp_environment_t;
struct mem_arena_t;

enum
{
	LUSP_COMPILE_DEBUG_INFO = 1 << 0,
	LUSP_COMPILE_OPTIMIZE = 1 << 1,

	LUSP_COMPILE_DEFAULT = LUSP_COMPILE_DEBUG_INFO | LUSP_COMPILE_OPTIMIZE
};

struct lusp_object_t lusp_compile(struct lusp_environment_t* env, struct mem_arena_t* arena, const char* string, unsigned int flags);
