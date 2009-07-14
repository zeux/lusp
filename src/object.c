// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/object.h>

#include <core/string.h>
#include <mem/arena.h>

extern struct mem_arena_t g_lusp_heap;

static inline struct lusp_object_t* mkobject(enum lusp_object_type_t type)
{
	struct lusp_object_t* result = MEM_ARENA_NEW(&g_lusp_heap, struct lusp_object_t);
	DL_ASSERT(result);
	
	result->type = type;
	return result;
}

static inline const char* mkstring(const char* value)
{
	size_t length = str_length(value);
	
	char* result = MEM_ARENA_NEW_ARRAY(&g_lusp_heap, char, length + 1);
	DL_ASSERT(result);
	
	str_copy(result, length + 1, value);
	
	return result;
}

struct lusp_object_t* lusp_mksymbol(const char* name)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_SYMBOL);
	result->symbol.name = mkstring(name);
	return result;
}

struct lusp_object_t* lusp_mkboolean(bool value)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_BOOLEAN);
	result->boolean.value = value;
	return result;
}

struct lusp_object_t* lusp_mkinteger(int value)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_INTEGER);
	result->integer.value = value;
	return result;
}

struct lusp_object_t* lusp_mkreal(float value)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_REAL);
	result->real.value = value;
	return result;
}

struct lusp_object_t* lusp_mkstring(const char* value)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_STRING);
	result->string.value = mkstring(value);
	return result;
}

struct lusp_object_t* lusp_mkcons(struct lusp_object_t* car, struct lusp_object_t* cdr)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_CONS);
	result->cons.car = car;
	result->cons.cdr = cdr;
	return result;
}