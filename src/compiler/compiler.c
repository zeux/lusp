// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/compiler/compiler.h>

#include <lusp/compiler/parser.h>

#include <lusp/object.h>
#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/eval.h>
#include <lusp/memory.h>

#include <core/memory.h>

#include <setjmp.h>
#include <stdio.h>

struct compiler_t;

struct binding_t
{
	struct lusp_object_t* symbol;
	unsigned int index;
};

struct scope_t
{
	struct compiler_t* compiler;
	
	struct scope_t* parent;
	
	struct binding_t binds[1024];
	unsigned int bind_count;
};

struct upval_t
{
	struct scope_t* scope;
	struct binding_t* binding;
};

struct compiler_t
{
    // global environment
    struct lusp_environment_t* env;
    
	// scope stack
	struct scope_t* scope;
	
	// local variables
	unsigned int local_count;
	
	// upvalues
	struct upval_t upvals[1024];
	unsigned int upval_count;
	
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

static inline struct binding_t* find_bind_local(struct scope_t* scope, struct lusp_object_t* symbol)
{
	for (unsigned int i = 0; i < scope->bind_count; ++i)
		if (scope->binds[i].symbol == symbol)
			return &scope->binds[i];
	
	return 0;
}

static inline struct binding_t* find_bind(struct compiler_t* compiler, struct lusp_object_t* symbol, struct scope_t** scope)
{
	for (struct scope_t* current = compiler->scope; current; current = current->parent)
	{
		struct binding_t* result = find_bind_local(current, symbol);
		
		if (result)
		{
			*scope = current;
			return result;
		}		
	}
	
	return 0;
}

static inline unsigned int find_upval(struct compiler_t* compiler, struct scope_t* scope, struct binding_t* binding)
{
	// look for an existing upval
	for (unsigned int i = 0; i < compiler->upval_count; ++i)
		if (compiler->upvals[i].binding == binding)
		{
			DL_ASSERT(compiler->upvals[i].scope == scope);
			return i;
		}
		
	// add new upval
	compiler->upvals[compiler->upval_count].binding = binding;
	compiler->upvals[compiler->upval_count].scope = scope;
	
	return compiler->upval_count++;
}

static inline void emit(struct compiler_t* compiler, struct lusp_vm_op_t op)
{
	check(compiler, compiler->op_count < sizeof(compiler->ops) / sizeof(compiler->ops[0]), "op buffer overflow");
	
	compiler->ops[compiler->op_count++] = op;
}

static inline void fixup_jump(struct compiler_t* compiler, unsigned int jump, unsigned int dest)
{
	compiler->ops[jump].jump.offset = (int)dest - (int)jump;
}

static struct lusp_vm_bytecode_t* create_closure(struct compiler_t* compiler, struct compiler_t* parent, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body);
static void compile(struct compiler_t* compiler, struct lusp_ast_node_t* node);

static void compile_object(struct compiler_t* compiler, struct lusp_object_t* object)
{
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_GET_OBJECT;
	op.get_object.object = object;
	emit(compiler, op);
}

static void compile_literal(struct compiler_t* compiler, struct lusp_ast_node_t* node)
{
	DL_ASSERT(!node || node->type == LUSP_AST_LITERAL);
	
	compile_object(compiler, node ? node->literal : 0);
}

static void compile_bind_getset(struct compiler_t* compiler, struct scope_t* scope, struct binding_t* bind, bool set)
{
	if (scope->compiler == compiler)
	{
		// local variable
		struct lusp_vm_op_t op;
	
		op.opcode = set ? LUSP_VMOP_SET_LOCAL : LUSP_VMOP_GET_LOCAL;
		op.getset_local.index = bind->index;
		emit(compiler, op);
	}
	else
	{
		// upvalue
		struct lusp_vm_op_t op;
	
		op.opcode = set ? LUSP_VMOP_SET_UPVAL : LUSP_VMOP_GET_UPVAL;
		op.getset_local.index = find_upval(compiler, scope, bind);
		emit(compiler, op);
	}
}

static void compile_symbol_getset(struct compiler_t* compiler, struct lusp_object_t* symbol, bool set)
{
	DL_ASSERT(symbol && symbol->type == LUSP_OBJECT_SYMBOL);
	
	struct scope_t* scope;
	struct binding_t* bind = find_bind(compiler, symbol, &scope);
	
	if (bind)
	{
		// local variable or upvalue
		compile_bind_getset(compiler, scope, bind, set);
	}
	else
	{
		// global variable
		struct lusp_vm_op_t op;
	
		op.opcode = set ? LUSP_VMOP_SET_GLOBAL : LUSP_VMOP_GET_GLOBAL;
		op.getset_global.slot = lusp_environment_get_slot(compiler->env, symbol);
		emit(compiler, op);
	}
}

static void compile_symbol(struct compiler_t* compiler, struct lusp_ast_node_t* node)
{
	DL_ASSERT(node && node->type == LUSP_AST_SYMBOL);
	
    compile_symbol_getset(compiler, node->symbol, false);
}

static unsigned int compile_list(struct compiler_t* compiler, struct lusp_ast_node_t* object, bool push)
{
	unsigned int count = 0;
	
	while (object)
	{
		check(compiler, object->type == LUSP_AST_CONS, "invalid parameter");
	
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

static void compile_call(struct compiler_t* compiler, struct lusp_ast_node_t* func, struct lusp_ast_node_t* args)
{
	// evaluate function arguments left to right to stack
	unsigned int arg_count = compile_list(compiler, args, true);
	
	// evaluate function
	compile(compiler, func);
	
	// call function
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_CALL;
	op.call.count = arg_count;
	emit(compiler, op);
}

static void compile_whenunless(struct compiler_t* compiler, struct lusp_ast_node_t* args, bool unless)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "when/unless: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, cdr && cdr->type == LUSP_AST_CONS, "when/unless: malformed syntax");
	
	struct lusp_ast_node_t* cond = car;
	struct lusp_ast_node_t* code = cdr;
	
	// evaluate condition
	compile(compiler, cond);
	
	// jump over code
	unsigned int jump_op = compiler->op_count;

	struct lusp_vm_op_t op;

	op.opcode = unless ? LUSP_VMOP_JUMP_IF : LUSP_VMOP_JUMP_IFNOT;
	op.jump.offset = 0;
	emit(compiler, op);

	// evaluate code
	compile_list(compiler, code, false);

	// fixup jump
	fixup_jump(compiler, jump_op, compiler->op_count);
}

static void compile_if(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "if: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, cdr && cdr->type == LUSP_AST_CONS, "if: malformed syntax");
	check(compiler, !cdr->cons.cdr || cdr->cons.cdr->cons.cdr == 0, "if: malformed syntax");
	
	struct lusp_ast_node_t* cond = car;
	struct lusp_ast_node_t* ifcode = cdr->cons.car;
	struct lusp_ast_node_t* elsecode = cdr->cons.cdr ? cdr->cons.cdr->cons.car : 0;
	
	// evaluate condition
	compile(compiler, cond);
	
	// jump over if code
	unsigned int jump_ifnot_op = compiler->op_count;
	
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_JUMP_IFNOT;
	op.jump.offset = ~0u;
	emit(compiler, op);
	
	// evaluate if code
	compile(compiler, ifcode);
	
	// jump over else code
	unsigned int jump_op = compiler->op_count;
	
	op.opcode = LUSP_VMOP_JUMP;
	op.jump.offset = ~0u;
	emit(compiler, op);
	
	// else code
	compile(compiler, elsecode);
	
	// fixup jumps
	fixup_jump(compiler, jump_ifnot_op, jump_op + 1);
	fixup_jump(compiler, jump_op, compiler->op_count);
}

static void compile_set(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "set!: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, car && car->type == LUSP_AST_SYMBOL, "set!: first argument has to be a symbol");
	check(compiler, cdr && cdr->type == LUSP_AST_CONS && cdr->cons.cdr == 0, "set!: malformed syntax");
	
	// evaluate value
	compile(compiler, cdr->cons.car);
	
	// set value
    compile_symbol_getset(compiler, car->symbol, true);
}

static void compile_let_pushvardecl(struct compiler_t* compiler, struct scope_t* scope, struct lusp_ast_node_t* vardecl)
{
    check(compiler, vardecl && vardecl->type == LUSP_AST_CONS, "let: malformed syntax");

    struct lusp_ast_node_t* var = vardecl->cons.car;
    struct lusp_ast_node_t* decl = vardecl->cons.cdr;

    check(compiler, var && var->type == LUSP_AST_SYMBOL, "let: malformed syntax");
    check(compiler, decl && decl->type == LUSP_AST_CONS && decl->cons.cdr == 0, "let: malformed syntax");

    // add value to new scope
    unsigned int index = compiler->local_count++;
    
    check(compiler, !find_bind_local(scope, var->symbol), "let: duplicate arguments detected");
    
    scope->binds[scope->bind_count].symbol = var->symbol;
    scope->binds[scope->bind_count].index = index;
    scope->bind_count++;

    // compute value in the old scope
    compile(compiler, decl->cons.car);

    // store value in locals
    struct lusp_vm_op_t op;

    op.opcode = LUSP_VMOP_SET_LOCAL;
    op.getset_local.index = index;
    emit(compiler, op);
}

static void compile_letseq_helper(struct compiler_t* compiler, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
    // just compile the body if there are no arguments
	if (!args)
	{
	    compile_list(compiler, body, false);
	    return;
	}
	
	// add new scope
	struct scope_t scope;
	scope.compiler = compiler;
	scope.parent = compiler->scope;
	scope.bind_count = 0;
	
	// add first binding
	check(compiler, args && args->type == LUSP_AST_CONS, "let*: malformed syntax");
	
	compile_let_pushvardecl(compiler, &scope, args->cons.car);
	
	// evaluate the rest recursively in the new scope
    compiler->scope = &scope;
	compile_letseq_helper(compiler, args->cons.cdr, body);
	compiler->scope = scope.parent;
}

static void compile_letseq(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "let*: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	compile_letseq_helper(compiler, car, cdr);
}

static void compile_let(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "let: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, !car || car->type == LUSP_AST_CONS, "let: first argument has to be a list");
	
	// add new scope
	struct scope_t scope;
	scope.compiler = compiler;
	scope.parent = compiler->scope;
	scope.bind_count = 0;
	
	// fill new scope with arguments and compute values
	for (struct lusp_ast_node_t* vdlist = car; vdlist; vdlist = vdlist->cons.cdr)
	{
		check(compiler, vdlist->type == LUSP_AST_CONS, "let: malformed syntax");
		
		compile_let_pushvardecl(compiler, &scope, vdlist->cons.car);
	}
	
	// evaluate body in new scope
	compiler->scope = &scope;
	compile_list(compiler, cdr, false);
	compiler->scope = scope.parent;
}

static void compile_closure(struct compiler_t* parent, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
	struct compiler_t compiler;
	
	// compile closure
	struct lusp_vm_bytecode_t* bytecode = create_closure(&compiler, parent, args, body);
	
	struct lusp_vm_op_t op;
	
	// create closure
	op.opcode = LUSP_VMOP_CREATE_CLOSURE;
	op.create_closure.code = bytecode;
	emit(parent, op);
	
	// set upvalues
	for (unsigned int i = 0; i < compiler.upval_count; ++i)
	{
		struct upval_t u = compiler.upvals[i];
		
		compile_bind_getset(parent, u.scope, u.binding, false);
	}
}

static void compile_lambda(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "lambda: malformed syntax");
	
	compile_closure(compiler, args->cons.car, args->cons.cdr);
}

static void compile_define(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "define: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, car && (car->type == LUSP_AST_SYMBOL || car->type == LUSP_AST_CONS),
		"define: first argument has to be either symbol or list");
	check(compiler, car->type == LUSP_AST_SYMBOL || car->cons.car->type == LUSP_AST_SYMBOL,
		"define: function declaration has to start with symbol");
	
	// get actual symbol
	struct lusp_object_t* symbol = (car->type == LUSP_AST_SYMBOL) ? car->symbol : car->cons.car->symbol;
	
	// compile closure/value
	if (car->type == LUSP_AST_CONS)
    	compile_closure(compiler, car->cons.cdr, cdr);
	else
	{
		check(compiler, !cdr || (cdr->type == LUSP_AST_CONS && cdr->cons.cdr == 0), "define: malformed syntax");
		
		compile(compiler, cdr ? cdr->cons.car : 0);
	}
	
	// set value to slot
	struct lusp_vm_op_t op;
	
	op.opcode = LUSP_VMOP_SET_GLOBAL; // $$$ support local defines
	op.getset_global.slot = lusp_environment_get_slot(compiler->env, symbol);
	emit(compiler, op);
}

static struct lusp_object_t* compile_quote_helper(struct compiler_t* compiler, struct lusp_ast_node_t* node)
{
	if (!node) return 0;
	
	switch (node->type)
	{
	case LUSP_AST_LITERAL:
		return node->literal;
		
	case LUSP_AST_CONS:
		return lusp_mkcons(compile_quote_helper(compiler, node->cons.car), compile_quote_helper(compiler, node->cons.cdr));
	
	default:
		check(compiler, node->type >= LUSP_AST_SYMBOL, "quote: malformed syntax");
		
		return node->symbol;
	}
}

static void compile_quote(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS && args->cons.cdr == 0, "quote: malformed syntax");
	
	compile_object(compiler, compile_quote_helper(compiler, args->cons.car));
}

static void compile_begin(struct compiler_t* compiler, struct lusp_ast_node_t* args)
{
	compile_list(compiler, args, false);
}

static void compile_cons(struct compiler_t* compiler, struct lusp_ast_node_t* node)
{
	DL_ASSERT(node && node->type == LUSP_AST_CONS);
	
	struct lusp_ast_node_t* func = node->cons.car;
	struct lusp_ast_node_t* args = node->cons.cdr;
	
	check(compiler, func != 0, "invalid function call");
	
	switch (func->type)
	{
	case LUSP_AST_SYMBOL:
	case LUSP_AST_CONS:
		return compile_call(compiler, func, args);
		
	case LUSP_AST_SYMBOL_QUOTE:
		return compile_quote(compiler, args);
		
	case LUSP_AST_SYMBOL_BEGIN:
		return compile_begin(compiler, args);
		
	case LUSP_AST_SYMBOL_DEFINE:
		return compile_define(compiler, args);
		
	case LUSP_AST_SYMBOL_LAMBDA:
		return compile_lambda(compiler, args);
		
	case LUSP_AST_SYMBOL_LET:
		return compile_let(compiler, args);
		
	case LUSP_AST_SYMBOL_LETSEQ:
		return compile_letseq(compiler, args);
		
	case LUSP_AST_SYMBOL_SET:
		return compile_set(compiler, args);
		
	case LUSP_AST_SYMBOL_IF:
		return compile_if(compiler, args);
		
	case LUSP_AST_SYMBOL_WHEN:
		return compile_whenunless(compiler, args, false);
		
	case LUSP_AST_SYMBOL_UNLESS:
		return compile_whenunless(compiler, args, true);
		
	default:
		check(compiler, false, "invalid function call");
	}
}

static void compile(struct compiler_t* compiler, struct lusp_ast_node_t* node)
{
	if (!node) return compile_object(compiler, 0);
	
	switch (node->type)
	{
	case LUSP_AST_SYMBOL:
		return compile_symbol(compiler, node);
	
	case LUSP_AST_LITERAL:
		return compile_literal(compiler, node);
		
	case LUSP_AST_CONS:
		return compile_cons(compiler, node);
 
	default:
		check(compiler, false, "unknown node type");
	}
}

static void compile_closure_code(struct compiler_t* compiler, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
    // add top-level scope
    struct scope_t scope;
    scope.compiler = compiler;
    scope.parent = compiler->scope;
    scope.bind_count = 0;

    compiler->scope = &scope;
    
    // fill scope with arguments
    bool rest = false;
    
    for (struct lusp_ast_node_t* arg = args; arg; arg = arg->cons.cdr)
    {
        check(compiler, arg->type == LUSP_AST_CONS || arg->type == LUSP_AST_SYMBOL, "lambda: malformed syntax");
        check(compiler, arg->type == LUSP_AST_SYMBOL || (arg->cons.car && arg->cons.car->type == LUSP_AST_SYMBOL), "lambda: malformed syntax");
        
        rest = (arg->type == LUSP_AST_SYMBOL);
        
        struct lusp_object_t* symbol = (arg->type == LUSP_AST_CONS) ? arg->cons.car->symbol : arg->symbol;
        
        check(compiler, !find_bind_local(&scope, symbol), "lambda: duplicate arguments detected");

        scope.binds[scope.bind_count].symbol = symbol;
        scope.binds[scope.bind_count].index = compiler->local_count++;
        scope.bind_count++;
        
        if (rest) break;
    }
    
    // create list for rest
    if (rest)
    {
        struct lusp_vm_op_t op;
        
        op.opcode = LUSP_VMOP_CREATE_LIST;
        op.create_list.index = scope.bind_count - 1;
        emit(compiler, op);
    }
    
    // compile body
    compile_list(compiler, body, false);
    
    // return
    struct lusp_vm_op_t op;
    
    op.opcode = LUSP_VMOP_RETURN;
    emit(compiler, op);
}

static struct lusp_vm_bytecode_t* create_closure(struct compiler_t* compiler, struct compiler_t* parent, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
    // create compiler
    compiler->env = parent->env;
    compiler->scope = parent->scope;
    compiler->local_count = 0;
    compiler->upval_count = 0;
    compiler->op_count = 0;
    compiler->error = parent->error;
    
    // compile closure code
    compile_closure_code(compiler, args, body);
    
    // create new closure
    unsigned int ops_size = compiler->op_count * sizeof(struct lusp_vm_op_t);
    
    struct lusp_vm_op_t* ops = (struct lusp_vm_op_t*)lusp_memory_allocate(ops_size);
    DL_ASSERT(ops);
    
    memcpy(ops, compiler->ops, ops_size);

    struct lusp_vm_bytecode_t* code = (struct lusp_vm_bytecode_t*)lusp_memory_allocate(sizeof(struct lusp_vm_bytecode_t));
    DL_ASSERT(code);
    
    code->env = parent->env;
    code->local_count = compiler->local_count;
    code->upval_count = compiler->upval_count;
    code->ops = ops;
    code->op_count = compiler->op_count;
	code->jit = 0;
	
	lusp_bytecode_setup(code);
	
    return code;
}

struct lusp_object_t* lusp_compile(struct lusp_environment_t* env, struct lusp_ast_node_t* node)
{
	// setup error handling facilities
	jmp_buf buf;
	
	if (setjmp(buf)) return 0;
	
	// create compiler 
	struct compiler_t parent;
	
    parent.env = env;
    parent.scope = 0;
    parent.error = &buf;
    
    // compile bytecode
    struct compiler_t compiler;
    
	struct lusp_vm_bytecode_t* bytecode = create_closure(&compiler, &parent, 0, node);
	
	// check correctness
	DL_ASSERT(compiler.upval_count == 0);
	
	// create resulting closure
	return lusp_mkclosure(bytecode, 0);
}
