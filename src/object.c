// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/object.h>

#include <stdlib.h>
#include <string.h>

static inline struct lusp_object_t* mkobject(enum lusp_object_type_t type)
{
	struct lusp_object_t* result = (struct lusp_object_t*)malloc(sizeof(struct lusp_object_t));
	DL_ASSERT(result);
	
	result->type = type;
	return result;
}

static inline const char* mkstring(const char* value)
{
	return strdup(value);
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

struct lusp_object_t* lusp_mkclosure(struct lusp_object_t* environment, struct lusp_object_t* args, struct lusp_object_t* body)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_CLOSURE);
	result->closure.environment = environment;
	result->closure.environment = environment;
	result->closure.args = args;
	result->closure.body = body;
	return result;
}

struct lusp_object_t* lusp_mkprocedure(void* context, lusp_procedure_t body)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_PROCEDURE);
	result->procedure.context = context;
	result->procedure.body = body;
	return result;
}

struct lusp_object_t* lusp_mkenvironment(struct lusp_object_t* parent, struct lusp_object_t* contents)
{
	struct lusp_object_t* result = mkobject(LUSP_OBJECT_ENVIRONMENT);
	result->environment.parent = parent;
	result->environment.contents = contents;
	return result;
}