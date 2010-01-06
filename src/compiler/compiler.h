// DeepLight Engine (c) Zeux 2006-2010

#pragma once

#include <lusp/object.h>

struct mem_arena_t;
struct lusp_lexer_t;

struct lusp_object_t lusp_compile_ex(struct lusp_environment_t* env, struct lusp_lexer_t* lexer, struct mem_arena_t* arena, unsigned int flags);
