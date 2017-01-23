#include "object.h"

#include "bytecode.h"
#include "memory.h"

#include <assert.h>
#include <string.h>

static struct lusp_symbol_t* g_lusp_symbols[1024];

static inline const char* mkstring(const char* value)
{
	size_t length = strlen(value);

	char* result = (char*)lusp_memory_allocate(length + 1);
	assert(result);

	strcpy(result, value);

	return result;
}

static uint32_t hash_string(const char* string)
{
	// Jenkins one-at-a-time hash
	// reference: http://en.wikipedia.org/wiki/Jenkins_hash_function#one-at-a-time
	uint32_t result = 0;

	while (*string)
	{
		result += *string;
		result += result << 10;
		result ^= result >> 6;
	}

	result += result << 3;
	result ^= result >> 11;
	result += result << 15;

	return result;
}

bool lusp_object_init()
{
	// intialize symbol hash table
	memset(g_lusp_symbols, 0, sizeof(g_lusp_symbols));

	return true;
}

void lusp_object_term()
{
}

struct lusp_object_t lusp_mknull()
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_NULL;
	return result;
}

struct lusp_object_t lusp_mksymbol(const char* name)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_SYMBOL;

	// compute hash
	const unsigned int hash_mask = sizeof(g_lusp_symbols) / sizeof(g_lusp_symbols[0]) - 1;
	unsigned int hash = hash_string(name) & hash_mask;

	// table lookup
	for (struct lusp_symbol_t* symbol = g_lusp_symbols[hash]; symbol; symbol = symbol->next)
		if (strcmp(name, symbol->name) == 0)
		{
			result.symbol = symbol;
			return result;
		}

	// construct new symbol
	struct lusp_symbol_t* symbol = (struct lusp_symbol_t*)lusp_memory_allocate(sizeof(struct lusp_symbol_t));
	symbol->name = mkstring(name);

	// insert symbol into hash table
	symbol->next = g_lusp_symbols[hash];
	g_lusp_symbols[hash] = symbol;

	result.symbol = symbol;
	return result;
}

struct lusp_object_t lusp_mkboolean(bool value)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_BOOLEAN;
	result.boolean = value;
	return result;
}

struct lusp_object_t lusp_mkinteger(int value)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_INTEGER;
	result.integer = value;
	return result;
}

struct lusp_object_t lusp_mkreal(float value)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_REAL;
	result.real = value;
	return result;
}

struct lusp_object_t lusp_mkstring(const char* value)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_STRING;
	result.string = mkstring(value);
	return result;
}

struct lusp_object_t lusp_mkcons(struct lusp_object_t car, struct lusp_object_t cdr)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_CONS;
	result.cons = (struct lusp_object_t*)lusp_memory_allocate(sizeof(struct lusp_object_t) * 2);
	result.cons[0] = car;
	result.cons[1] = cdr;
	return result;
}

struct lusp_object_t lusp_mkclosure(struct lusp_vm_bytecode_t* code, unsigned int upval_count)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_CLOSURE;

	result.closure = (struct lusp_vm_closure_t*)lusp_memory_allocate(sizeof(struct lusp_vm_closure_t) - sizeof(struct lusp_vm_upval_t*) + sizeof(struct lusp_vm_upval_t*) * upval_count);
	assert(result.closure);

	result.closure->code = code;
	return result;
}

struct lusp_object_t lusp_mkfunction(lusp_function_t code)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_FUNCTION;
	result.function = code;
	return result;
}

struct lusp_object_t lusp_mkobject(void* object)
{
	struct lusp_object_t result;
	result.type = LUSP_OBJECT_OBJECT;
	result.object = object;
	return result;
}
