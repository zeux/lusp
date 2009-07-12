// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/read.h>
#include <lusp/write.h>
#include <lusp/object.h>

#include <core/string.h>

#include <stdio.h>

static struct lusp_object_t* get_env_entry_local(struct lusp_object_t* env, const char* name)
{
	DL_ASSERT(env->type == LUSP_OBJECT_ENVIRONMENT);
	
	struct lusp_object_t* e = env->environment.contents;
	
	while (e)
	{
		DL_ASSERT(e->type == LUSP_OBJECT_CONS);
		DL_ASSERT(e->cons.car && e->cons.car->type == LUSP_OBJECT_SYMBOL);
		DL_ASSERT(e->cons.cdr && e->cons.cdr->type == LUSP_OBJECT_CONS);
		
		if (str_is_equal(name, e->cons.car->symbol.name)) return e;
		
		e = e->cons.cdr->cons.cdr;
	}
	
	return 0;
}

static struct lusp_object_t* get_env_entry(struct lusp_object_t* env, const char* name)
{
	while (env)
	{
		struct lusp_object_t* e = get_env_entry_local(env, name);
		
		if (e) return e;
		
		DL_ASSERT(env->type == LUSP_OBJECT_ENVIRONMENT);
		env = env->environment.parent;
	}
	
	return 0;
}

static void put_env(struct lusp_object_t* env, struct lusp_object_t* symbol, struct lusp_object_t* object)
{
	DL_ASSERT(symbol && symbol->type == LUSP_OBJECT_SYMBOL);
	
	struct lusp_object_t* e = get_env_entry_local(env, symbol->symbol.name);
	
	if (e)
	{
		// modify in place
		e->cons.cdr->cons.car = object;
	}
	else
	{
		// add new entry
		env->environment.contents = lusp_mkcons(symbol, lusp_mkcons(object, env->environment.contents));
	}
}

void set_env(struct lusp_object_t* env, struct lusp_object_t* symbol, struct lusp_object_t* object)
{
	DL_ASSERT(symbol && symbol->type == LUSP_OBJECT_SYMBOL);
	
	struct lusp_object_t* e = get_env_entry(env, symbol->symbol.name);
	
	if (e)
	{
		// modify in place
		e->cons.cdr->cons.car = object;
	}
	else
	{
		printf("error: no entry to set (%s)\n", symbol->symbol.name);
	}
}

static struct lusp_object_t* eval_list(struct lusp_object_t* env, struct lusp_object_t* object)
{
	struct lusp_object_t* head = 0;
	struct lusp_object_t* tail = 0;
	
	while (object)
	{
		DL_ASSERT(object->type == LUSP_OBJECT_CONS);
		
		// evaluate result
		struct lusp_object_t* result = lusp_eval(env, object->cons.car);
		
		// append result
		if (tail) tail = tail->cons.cdr = lusp_mkcons(result, 0);
		else head = tail = lusp_mkcons(result, 0);
		
		object = object->cons.cdr;
	}
	
	return head;
}

static struct lusp_object_t* eval_symbol(struct lusp_object_t* env, struct lusp_object_t* object)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_SYMBOL);
	
	struct lusp_object_t* e = get_env_entry(env, object->symbol.name);
	
	if (!e)
	{
		printf("error: symbol %s not found\n", object->symbol.name);
	}
	
	return e ? e->cons.cdr->cons.car : 0;
}

static struct lusp_object_t* eval_closure(struct lusp_object_t* env, struct lusp_object_t* object, struct lusp_object_t* args)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_CLOSURE);
	
	struct lusp_object_t* callenv = lusp_mkenvironment(object->closure.environment, 0);
	
	// add arguments to env
	struct lusp_object_t* argnames = object->closure.args;
	
	while (argnames && argnames->type == LUSP_OBJECT_CONS)
	{
		DL_ASSERT(args && args->type == LUSP_OBJECT_CONS);
		
		put_env(callenv, argnames->cons.car, lusp_eval(env, args->cons.car));
		
		argnames = argnames->cons.cdr;
		args = args->cons.cdr;
	}
	
	// add optional arguments, if any
	if (argnames) put_env(callenv, argnames, eval_list(env, args));
	else DL_ASSERT(!args);
	
	// evaluate body
	struct lusp_object_t* body = object->closure.body;
	
	struct lusp_object_t* result = 0;
	
	while (body)
	{
		DL_ASSERT(body->type == LUSP_OBJECT_CONS);
		
		result = lusp_eval(callenv, body->cons.car);
		
		body = body->cons.cdr;
	}
	
	return result;
}

static struct lusp_object_t* eval_procedure(struct lusp_object_t* env, struct lusp_object_t* object, struct lusp_object_t* args)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_PROCEDURE);
	
	return object->procedure.body(object->procedure.context, env, args);
}

static struct lusp_object_t* eval_cons(struct lusp_object_t* env, struct lusp_object_t* object)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_CONS);
	
	struct lusp_object_t* func = lusp_eval(env, object->cons.car);
	struct lusp_object_t* args = object->cons.cdr;
	
	if (!func)
	{
		printf("error: can't execute null function\n");
		return 0;
	}
	
	switch (func->type)
	{
	case LUSP_OBJECT_CLOSURE:
		return eval_closure(env, func, args);
		
	case LUSP_OBJECT_PROCEDURE:
		return eval_procedure(env, func, args);
	}
	
	printf("error: function type is %d\n", func->type);
	return 0;
}

struct lusp_object_t* lusp_eval(struct lusp_object_t* env, struct lusp_object_t* object)
{
	if (!object) return object;
	
	switch (object->type)
	{
	case LUSP_OBJECT_SYMBOL:
		return eval_symbol(env, object);
		
	case LUSP_OBJECT_BOOLEAN:
	case LUSP_OBJECT_INTEGER:
	case LUSP_OBJECT_REAL:
	case LUSP_OBJECT_STRING:
	case LUSP_OBJECT_CLOSURE:
	case LUSP_OBJECT_PROCEDURE:
		return object;
		
	case LUSP_OBJECT_CONS:
		return eval_cons(env, object);
	
	default:
		printf("error; unknown type\n");
		return 0;
	}
}
