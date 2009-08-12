// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/object.h>

#include <lusp/vm.h>
#include <lusp/memory.h>

#include <core/string.h>
#include <core/memory.h>
#include <core/hash.h>

struct lusp_object_t g_lusp_true;
struct lusp_object_t g_lusp_false;

static struct lusp_object_t* g_lusp_symbols[1024];

static inline struct lusp_object_t* mkobject(enum lusp_object_type_t type)
{
	struct lusp_object_t* result = (struct lusp_object_t*)lusp_memory_allocate(sizeof(struct lusp_object_t));
	DL_ASSERT(result);
	
	result->type = type;
	return result;
}

static inline const char* mkstring(const char* value)
{
	size_t length = str_length(value);
	
	char* result = (char*)lusp_memory_allocate(length + 1);
	DL_ASSERT(result);
	
	str_copy(result, length + 1, value);
	
	return result;
}

bool lusp_object_init()
{
	// initialize builtin boolean values
	g_lusp_true.type = LUSP_OBJECT_BOOLEAN;
	g_lusp_true.boolean.value = true;

	g_lusp_false.type = LUSP_OBJECT_BOOLEAN;
	g_lusp_false.boolean.value = false;

	// intialize symbol hash table
	memset(g_lusp_symbols, 0, sizeof(g_lusp_symbols));
	
	return true;
}

void lusp_object_term()
{
}

struct lusp_object_t* lusp_mksymbol(const char* name)
{
	// compute hash
	const unsigned int hash_mask = sizeof(g_lusp_symbols) / sizeof(g_lusp_symbols[0]) - 1;
	unsigned int hash = core_hash_string(name) & hash_mask;
	
	// table lookup
	for (struct lusp_object_t* object = g_lusp_symbols[hash]; object; object = object->symbol.next)
		if (str_is_equal(name, object->symbol.name))
			return object;
			
	// construct new object
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_SYMBOL);
	result->symbol.name = mkstring(name);
	
	// insert object into hash table
	result->symbol.next = g_lusp_symbols[hash];
	g_lusp_symbols[hash] = result;
	
	return result;
}

struct lusp_object_t* lusp_mkboolean(bool value)
{
	return value ? &g_lusp_true : &g_lusp_false;
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

struct lusp_object_t* lusp_mkclosure(struct lusp_vm_bytecode_t* code, unsigned int upval_count)
{
    struct lusp_object_t* result = mkobject(LUSP_OBJECT_CLOSURE);
    
	result->closure.closure = (struct lusp_vm_closure_t*)lusp_memory_allocate(sizeof(struct lusp_vm_upval_t*) * upval_count);
	DL_ASSERT(result->closure.closure);
	
    result->closure.code = code;
    return result;
}

struct lusp_object_t* lusp_mkprocedure(lusp_procedure_t code)
{
    struct lusp_object_t* result = mkobject(LUSP_OBJECT_PROCEDURE);
    result->procedure.code = code;
    return result;
}