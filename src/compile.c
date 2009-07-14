// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/compile.h>

#include <lusp/object.h>
#include <lusp/vm.h>

#include <core/string.h>
#include <core/memory.h>
#include <mem/arena.h>

#include <setjmp.h>
#include <stdio.h>

extern struct mem_arena_t g_lusp_heap;

struct binding_t
{
	const char* name;
};

struct scope_t
{
	struct scope_t* parent;
	
	struct binding_t binds[1024];
	unsigned int bind_count;
};

struct compiler_t
{
	// scope stack
	struct scope_t* scope;
	
	// opcode buffer
	struct lusp_vm_op_t ops[1024];
	unsigned int op_count;
	
	// error facilities
	jmp_buf* error;
};

static inline void check(struct compiler_t* compiler, bool condition, const char* message)
{
	if (!condition)
	{
		printf("error: compile failed (%s)\n", message);

		longjmp(*compiler->error, 1);
	}
}

static inline bool find_bind_local(struct scope_t* scope, const char* name, unsigned int* index)
{
	for (unsigned int i = 0; i < scope->bind_count; ++i)
		if (str_is_equal(scope->binds[i].name, name))
		{
			*index = i;
			return true;
		}
	
	return false;
}

static inline bool find_bind(struct compiler_t* compiler, const char* name, unsigned int* depth, unsigned int* index)
{
	*depth = 0;
	
	struct scope_t* scope = compiler->scope;
	
	while (scope)
	{
		if (find_bind_local(scope, name, index)) return true;
		
		*depth = *depth + 1;
		scope = scope->parent;
	}
	
	return false;
}

static inline void emit(struct compiler_t* compiler, struct lusp_vm_op_t op)
{
	check(compiler, compiler->op_count < sizeof(compiler->ops) / sizeof(compiler->ops[0]), "op buffer overflow");
	
	compiler->ops[compiler->op_count++] = op;
}

static void compile(struct compiler_t* compiler, struct lusp_object_t* object);

static void compile_object(struct compiler_t* compiler, struct lusp_object_t* object)
{
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_GET_OBJECT;
	op.get_object.object = object;
	emit(compiler, op);
}

static void compile_selfeval(struct compiler_t* compiler, struct lusp_object_t* object)
{
	DL_ASSERT(!object || object->type == LUSP_OBJECT_BOOLEAN || object->type == LUSP_OBJECT_INTEGER ||
		object->type == LUSP_OBJECT_REAL || object->type == LUSP_OBJECT_STRING);
	
	compile_object(compiler, object);
}

static void compile_symbol(struct compiler_t* compiler, struct lusp_object_t* object)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_SYMBOL);
	
	unsigned int local_depth, local_index;
	
	if (find_bind(compiler, object->symbol.name, &local_depth, &local_index))
	{
		struct lusp_vm_op_t op;
	
		op.opcode = LUSP_VMOP_GET_LOCAL;
		op.getset_local.depth = local_depth;
		op.getset_local.index = local_index;
		emit(compiler, op);
	}
	else
	{
		struct lusp_vm_op_t op;
	
		op.opcode = LUSP_VMOP_GET_GLOBAL;
		op.getset_global.index = 0; // $$$
		emit(compiler, op);
	}
}

static unsigned int compile_list(struct compiler_t* compiler, struct lusp_object_t* object, bool push)
{
	unsigned int count = 0;
	
	while (object)
	{
		check(compiler, object->type == LUSP_OBJECT_CONS, "invalid parameter");
	
		// compile list element
		compile(compiler, object->cons.car);
		
		// push element on stack
		if (push)
		{
			struct lusp_vm_op_t op;
			
			op.opcode = LUSP_VMOP_PUSH;
			emit(compiler, op);
		}
		
		count++;
		object = object->cons.cdr;
	}
	
	return count;
}

static void compile_call(struct compiler_t* compiler, struct lusp_object_t* func, struct lusp_object_t* args)
{
	struct lusp_vm_op_t op;
	
	// push current continuation
	op.opcode = LUSP_VMOP_PUSH_CONTINUATION;
	emit(compiler, op);
	
	// evaluate function arguments left to right to stack
	unsigned int arg_count = compile_list(compiler, args, true);
	
	// bind function arguments as a new environment frame
	op.opcode = LUSP_VMOP_BIND;
	op.bind.count = arg_count;
	emit(compiler, op);
	
	// evaluate function
	compile(compiler, func);
	
	// call function
	op.opcode = LUSP_VMOP_CALL;
	emit(compiler, op);
}

static void compile_closure(struct compiler_t* compiler, struct lusp_object_t* args, struct lusp_object_t* body)
{
	// add new scope
	struct scope_t scope;
	scope.parent = compiler->scope;
	scope.bind_count = 0;
	
	// fill new scope with arguments
	while (args)
	{
		check(compiler, args->type == LUSP_OBJECT_CONS && args->cons.car && args->cons.car->type == LUSP_OBJECT_SYMBOL,
			"lambda: malformed syntax");
			
		scope.binds[scope.bind_count++].name = args->cons.car->symbol.name; // $$$: detect duplicates
		args = args->cons.cdr;
	}
	
	// evaluate body in new scope
	compiler->scope = &scope;
	compile_list(compiler, body, false);
	compiler->scope = scope.parent;
	
	// return
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_RETURN;
	emit(compiler, op);
}

static void compile_lambda(struct compiler_t* compiler, struct lusp_object_t* args)
{
	check(compiler, args && args->type == LUSP_OBJECT_CONS, "lambda: malformed syntax");
	
	struct lusp_object_t* car = args->cons.car;
	struct lusp_object_t* cdr = args->cons.cdr;
	
	compile_closure(compiler, car, cdr);
}

static void compile_define(struct compiler_t* compiler, struct lusp_object_t* args)
{
	check(compiler, args && args->type == LUSP_OBJECT_CONS, "define: malformed syntax");
	
	struct lusp_object_t* car = args->cons.car;
	struct lusp_object_t* cdr = args->cons.cdr;
	
	check(compiler, car && (car->type == LUSP_OBJECT_SYMBOL || car->type == LUSP_OBJECT_CONS),
		"define: first argument has to be either symbol or list");
	check(compiler, car->type == LUSP_OBJECT_SYMBOL || car->cons.car->type == LUSP_OBJECT_SYMBOL,
		"define: function declaration has to start with symbol");
	
	// get actual name
	const char* name = (car->type == LUSP_OBJECT_SYMBOL) ? car->symbol.name : car->cons.car->symbol.name;
	
	// reserve slot in scope
	unsigned int depth = 0; // $$$
	unsigned int index = 0; // $$$
	
	// compile closure/value
	if (car->type == LUSP_OBJECT_CONS)
		compile_closure(compiler, car->cons.cdr, cdr);
	else
	{
		check(compiler, !cdr || (cdr->type == LUSP_OBJECT_CONS && cdr->cons.cdr == 0), "define: malformed syntax");
		
		compile(compiler, cdr ? cdr->cons.car : 0);
	}
	
	// set value to slot
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_SET_LOCAL;
	op.getset_local.depth = depth;
	op.getset_local.index = index;
	emit(compiler, op);
}

static void compile_quote(struct compiler_t* compiler, struct lusp_object_t* args)
{
	compile_object(compiler, args);
}

static void compile_begin(struct compiler_t* compiler, struct lusp_object_t* args)
{
	compile_list(compiler, args, false);
}

static void compile_syntax(struct compiler_t* compiler, struct lusp_object_t* func, struct lusp_object_t* args)
{
	DL_ASSERT(func && func->type == LUSP_OBJECT_SYMBOL);
	
	const char* name = func->symbol.name;
	
	if (str_is_equal(name, "begin"))
		compile_begin(compiler, args);
	else if (str_is_equal(name, "quote"))
		compile_quote(compiler, args);
	else if (str_is_equal(name, "define"))
		compile_define(compiler, args);
	else if (str_is_equal(name, "lambda"))
		compile_lambda(compiler, args);
	else
		compile_call(compiler, func, args);
}

static void compile_cons(struct compiler_t* compiler, struct lusp_object_t* object)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_CONS);
	
	struct lusp_object_t* car = object->cons.car;
	struct lusp_object_t* cdr = object->cons.cdr;
	
	check(compiler, car && (car->type == LUSP_OBJECT_SYMBOL || car->type == LUSP_OBJECT_CONS), "illegal function");
	
	if (car->type == LUSP_OBJECT_SYMBOL)
		compile_syntax(compiler, car, cdr);
	else
		compile_call(compiler, car, cdr);
}

static void compile(struct compiler_t* compiler, struct lusp_object_t* object)
{
	if (!object) return compile_selfeval(compiler, object);
	
	switch (object->type)
	{
	case LUSP_OBJECT_SYMBOL:
		return compile_symbol(compiler, object);
	
	case LUSP_OBJECT_BOOLEAN:
	case LUSP_OBJECT_INTEGER:
	case LUSP_OBJECT_REAL:
	case LUSP_OBJECT_STRING:
		return compile_selfeval(compiler, object);
		
	case LUSP_OBJECT_CONS:
		return compile_cons(compiler, object);
 
	default:
		check(compiler, false, "unknown object type");
	}
}

static struct lusp_object_t* compile_program(struct compiler_t* compiler, struct lusp_object_t* object)
{
	// init compiler
	struct scope_t scope;
	scope.parent = 0;
	scope.bind_count = 0;
	
	compiler->scope = &scope;
	
	compiler->op_count = 0;
	
	// compile expression
	compile(compiler, object);
	
	// create new closure
	struct lusp_vm_op_t* ops = MEM_ARENA_NEW_ARRAY(&g_lusp_heap, struct lusp_vm_op_t, compiler->op_count);
	memcpy(ops, compiler->ops, compiler->op_count * sizeof(struct lusp_vm_op_t));
	
	struct lusp_vm_bytecode_t* code = MEM_ARENA_NEW(&g_lusp_heap, struct lusp_vm_bytecode_t);
	code->ops = ops;
	code->count = compiler->op_count;
	
	struct lusp_object_t* result = MEM_ARENA_NEW(&g_lusp_heap, struct lusp_object_t);
	result->type = LUSP_OBJECT_CLOSURE;
	result->closure.env = 0;
	result->closure.code = code;
	
	return result;
}

struct lusp_object_t* lusp_compile(struct lusp_object_t* object)
{
	jmp_buf buf;
	struct compiler_t compiler;
	
	compiler.error = &buf;
	
	if (setjmp(buf)) return 0;
	
	return compile_program((struct compiler_t*)&compiler, object);
}
