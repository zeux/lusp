// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/environment.h>

#include <core/string.h>
#include <mem/arena.h>

extern struct mem_arena_t g_lusp_heap;

static inline const char* mkstring(const char* value)
{
    size_t length = str_length(value);

    char* result = MEM_ARENA_NEW_ARRAY(&g_lusp_heap, char, length + 1);
    DL_ASSERT(result);

    str_copy(result, length + 1, value);

    return result;
}

static struct lusp_environment_slot_t* mkslot(struct lusp_environment_slot_t* next, const char* name)
{
    struct lusp_environment_slot_t* result = MEM_ARENA_NEW(&g_lusp_heap, struct lusp_environment_slot_t);
    DL_ASSERT(result);
    
    result->name = mkstring(name);
    result->value = 0;
    result->next = next;
    
    return result;
}

struct lusp_environment_slot_t* find_slot(struct lusp_environment_t* env, const char* name)
{
    for (struct lusp_environment_slot_t* slot = env->head; slot; slot = slot->next)
        if (str_is_equal(slot->name, name))
            return slot;
    
    return 0;
}

struct lusp_environment_t* lusp_environment_create()
{
    struct lusp_environment_t* result = MEM_ARENA_NEW(&g_lusp_heap, struct lusp_environment_t);
    DL_ASSERT(result);
    
    result->head = 0;
    
    return result;
}

struct lusp_environment_slot_t* lusp_environment_get_slot(struct lusp_environment_t* env, const char* name)
{
    struct lusp_environment_slot_t* result = find_slot(env, name);
    
    return result ? result : (env->head = mkslot(env->head, name));
}

struct lusp_object_t* lusp_environment_get(struct lusp_environment_t* env, const char* name)
{
    return lusp_environment_get_slot(env, name)->value;
}

void lusp_environment_put(struct lusp_environment_t* env, const char* name, struct lusp_object_t* object)
{
    lusp_environment_get_slot(env, name)->value = object;
}