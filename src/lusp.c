#include "lusp.h"

#include "object.h"
#include "memory.h"
#include "eval.h"

bool lusp_init(struct mem_arena_t* arena, unsigned int heap_size)
{
	// initialize memory
	if (!lusp_memory_init(arena, heap_size)) return false;
	
	// initialize builtin objects
	if (!lusp_object_init()) return false;
	
	// disable JIT by default
	lusp_jit_set(false);
	
	return true;
}

void lusp_term()
{
    lusp_memory_term();
	lusp_object_term();
}
