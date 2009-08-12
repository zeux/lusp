// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/environment.h>

#include <lusp/object.h>
#include <lusp/memory.h>

static struct lusp_environment_slot_t* mkslot(struct lusp_environment_slot_t* next, struct lusp_object_t name)
{
	DL_ASSERT(name.type == LUSP_OBJECT_SYMBOL);
	
    struct lusp_environment_slot_t* result = (struct lusp_environment_slot_t*)lusp_memory_allocate(sizeof(struct lusp_environment_slot_t));
    DL_ASSERT(result);
    
    result->name = name.symbol;
    result->next = next;
    result->value = lusp_mknull();
    
    return result;
}

struct lusp_environment_slot_t* find_slot(struct lusp_environment_t* env, struct lusp_object_t name)
{
	DL_ASSERT(name.type == LUSP_OBJECT_SYMBOL);
	
    for (struct lusp_environment_slot_t* slot = env->head; slot; slot = slot->next)
        if (slot->name == name.symbol)
			return slot;
    
    return 0;
}

struct lusp_environment_t* lusp_environment_create()
{
    struct lusp_environment_t* result = (struct lusp_environment_t*)lusp_memory_allocate(sizeof(struct lusp_environment_t));
    DL_ASSERT(result);
    
    result->head = 0;
    
    return result;
}

struct lusp_environment_slot_t* lusp_environment_get_slot(struct lusp_environment_t* env, struct lusp_object_t name)
{
    struct lusp_environment_slot_t* result = find_slot(env, name);
    
    return result ? result : (env->head = mkslot(env->head, name));
}

struct lusp_object_t lusp_environment_get(struct lusp_environment_t* env, struct lusp_object_t name)
{
    return lusp_environment_get_slot(env, name)->value;
}

void lusp_environment_put(struct lusp_environment_t* env, struct lusp_object_t name, struct lusp_object_t object)
{
    lusp_environment_get_slot(env, name)->value = object;
}