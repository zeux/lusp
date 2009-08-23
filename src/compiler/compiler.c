// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/compiler/compiler.h>

#include <lusp/compiler/parser.h>

#include <lusp/object.h>
#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/eval.h>
#include <lusp/memory.h>
#include <lusp/compile.h>

#include <lusp/compiler/internal.h>
#include <lusp/compiler/codegen.h>

#include <core/memory.h>

static inline struct binding_t* find_bind_local(struct scope_t* scope, struct lusp_object_t symbol)
{
	for (unsigned int i = 0; i < scope->bind_count; ++i)
		if (scope->binds[i].symbol.symbol == symbol.symbol)
			return &scope->binds[i];
	
	return 0;
}

static inline struct binding_t* find_bind(struct compiler_t* compiler, struct lusp_object_t symbol, struct scope_t** scope)
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
		
	// mark binding
	DL_ASSERT(!binding->has_upval);
	binding->has_upval = true;
	
	// add new upval
	compiler->upvals[compiler->upval_count].binding = binding;
	compiler->upvals[compiler->upval_count].scope = scope;
	
	return compiler->upval_count++;
}

static inline void push_scope(struct compiler_t* compiler, struct scope_t* scope)
{
	scope->parent = compiler->scope;
	compiler->scope = scope;
}

static inline void pop_scope(struct compiler_t* compiler)
{
	struct scope_t* scope = compiler->scope;
	
	// look for bindings that need to be closed
	unsigned int upval_begin = ~0u, upval_end = 0;
	
	for (unsigned int i = 0; i < scope->bind_count; ++i)
		if (scope->binds[i].has_upval)
		{
			unsigned int index = scope->binds[i].index;
			
			upval_begin = min(upval_begin, index);
			upval_end = max(upval_end, index + 1);
		}
		
	// close bindings
	if (upval_begin < upval_end) emit_close(compiler, upval_begin, upval_end);
	
	compiler->scope = scope->parent;
}

static inline unsigned int allocate_registers(struct compiler_t* compiler, unsigned int count)
{
	unsigned int result = compiler->free_reg;
	compiler->free_reg += count;
	compiler->reg_count = max(compiler->reg_count, compiler->free_reg);
	return result;
}

static struct lusp_vm_bytecode_t* create_closure(struct compiler_t* compiler, struct compiler_t* parent, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body);
static void compile(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* node);

static void compile_object(struct compiler_t* compiler, unsigned int reg, struct lusp_object_t object)
{
	struct lusp_object_t o = lusp_mkcons(object, object);
	
	emit_load_const(compiler, reg, o.cons);
}

static void compile_literal(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* node)
{
	DL_ASSERT(!node || node->type == LUSP_AST_LITERAL);
	
	compile_object(compiler, reg, node ? node->literal : lusp_mknull());
}

static void compile_bind_getset(struct compiler_t* compiler, unsigned int reg, struct scope_t* scope, struct binding_t* bind, bool set)
{
	if (scope->compiler == compiler)
	{
		// local variable
		set ? emit_move(compiler, bind->index, reg) : emit_move(compiler, reg, bind->index);
	}
	else
	{
		// upvalue
		unsigned int index = find_upval(compiler, scope, bind);
		
		emit_loadstore_upval(compiler, reg, index, set);
	}
}

static void compile_symbol_getset(struct compiler_t* compiler, unsigned int reg, struct lusp_object_t symbol, bool set)
{
	DL_ASSERT(symbol.type == LUSP_OBJECT_SYMBOL);
	
	struct scope_t* scope;
	struct binding_t* bind = find_bind(compiler, symbol, &scope);
	
	if (bind)
	{
		// local variable or upvalue
		compile_bind_getset(compiler, reg, scope, bind, set);
	}
	else
	{
		// global variable
		emit_loadstore_global(compiler, reg, lusp_environment_get_slot(compiler->env, symbol), set);
	}
}

static void compile_symbol(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* node)
{
	DL_ASSERT(node && node->type == LUSP_AST_SYMBOL);
	
    compile_symbol_getset(compiler, reg, node->symbol, false);
}

static void compile_list(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* object, bool incr_reg)
{
	while (object)
	{
		check(compiler, object->type == LUSP_AST_CONS, "invalid parameter");
	
		// compile list element
		compile(compiler, reg, object->cons.car);
		
		// increment register
		reg += incr_reg;
		
		object = object->cons.cdr;
	}
}

static unsigned int list_length(struct lusp_ast_node_t* head)
{
	unsigned int result = 0;
	
	while (head && head->type == LUSP_AST_CONS)
	{
		result++;
		
		head = head->cons.cdr;
	}
	
	return result;
}

static void compile_call(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* func, struct lusp_ast_node_t* args)
{
	unsigned int free_reg = compiler->free_reg;
	
	// allocate registers for function arguments
	unsigned int arg_count = list_length(args);
	unsigned int arg_regs = allocate_registers(compiler, arg_count);
	
	// evaluate function arguments left to right
	compile_list(compiler, arg_regs, args, true);
	
	// evaluate function
	compile(compiler, reg, func);
	
	// call function
	emit_call(compiler, reg, arg_regs, arg_count);
	
	// free temporary registers
	compiler->free_reg = free_reg;
}

struct do_var_t
{
    struct lusp_ast_node_t* var;
    struct lusp_ast_node_t* init;
    struct lusp_ast_node_t* step;
};

static unsigned int extract_do_var(struct compiler_t* compiler, struct lusp_ast_node_t* init, struct do_var_t* dest)
{
    unsigned int count = 0;
    
    for (struct lusp_ast_node_t* it = init; it; it = it->cons.cdr)
    {
        check(compiler, it->type == LUSP_AST_CONS, "do: malformed syntax");
        
        struct lusp_ast_node_t* vis = it->cons.car;
        
        check(compiler, vis && vis->type == LUSP_AST_CONS, "do: malformed syntax");
        
        struct lusp_ast_node_t* var = vis->cons.car;
        struct lusp_ast_node_t* is = vis->cons.cdr;
        
        check(compiler, var && var->type == LUSP_AST_SYMBOL, "do: malformed syntax");
        check(compiler, is && is->type == LUSP_AST_CONS, "do: malformed syntax");
        
        struct lusp_ast_node_t* init = is->cons.car;
        struct lusp_ast_node_t* s = is->cons.cdr;
        
        check(compiler, s && s->type == LUSP_AST_CONS && !s->cons.cdr, "do: malformed syntax");
        
        struct lusp_ast_node_t* step = s->cons.car;
        
        // $$$ buffer overflow check
        dest[count].var = var;
        dest[count].init = init;
        dest[count].step = step;
        
        count++;
    }
    
    return count;
}

static void compile_do(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS && args->cons.cdr && args->cons.cdr->type == LUSP_AST_CONS, "do: malformed syntax");
	
	struct lusp_ast_node_t* init = args->cons.car;
	struct lusp_ast_node_t* test = args->cons.cdr->cons.car;
	struct lusp_ast_node_t* code = args->cons.cdr->cons.cdr;
	
	check(compiler, test && test->type == LUSP_AST_CONS, "do: malformed syntax");
	
	// extract do variables
	struct do_var_t vars[1024];
	unsigned int var_count = extract_do_var(compiler, init, vars);
	
    // add new scope
    struct scope_t scope;
    scope.compiler = compiler;
    scope.bind_count = 0;

    // process init steps
	unsigned int free_reg = compiler->free_reg;
	
    for (unsigned int i = 0; i < var_count; ++i)
    {
        check(compiler, !find_bind_local(&scope, vars[i].var->symbol), "do: duplicate variables detected");

		// reserve space for variable
		unsigned int index = allocate_registers(compiler, 1);
		
        // evaluate init step
        compile(compiler, index, vars[i].init);
        
        // add new variable to scope
        scope.binds[scope.bind_count].symbol = vars[i].var->symbol;
        scope.binds[scope.bind_count].index = index;
        scope.binds[scope.bind_count].has_upval = false;
        scope.bind_count++;
    }
    
    // push scope
    push_scope(compiler, &scope);
    
    // loop label
    unsigned int loop_op = compiler->op_count;
    
    // evaluate test
    compile(compiler, reg, test->cons.car);
    
    // test label
    unsigned int test_op = compiler->op_count;
    
    // jump to the exit if test is true
    emit_jump_if(compiler, reg, 0);
    
    // evaluate commands
    compile_list(compiler, reg, code, false);
    
    // evaluate steps
    for (unsigned int i = 0; i < var_count; ++i)
        compile(compiler, scope.binds[i].index, vars[i].step);
    
    // loop
    emit_jump(compiler, 0);
    
    // fixup jumps
    fixup_jump(compiler, compiler->op_count - 1, loop_op);
    fixup_jump(compiler, test_op, compiler->op_count);
    
    // evaluate exit expression
    compile_list(compiler, reg, test->cons.cdr, false);
    
    // pop scope
    pop_scope(compiler);
    
	// free registers
	compiler->free_reg = free_reg;
}

static void compile_whenunless(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args, bool unless)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "when/unless: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, cdr && cdr->type == LUSP_AST_CONS, "when/unless: malformed syntax");
	
	struct lusp_ast_node_t* cond = car;
	struct lusp_ast_node_t* code = cdr;
	
	// evaluate condition
	compile(compiler, reg, cond);
	
	// jump over code
	unsigned int jump_op = compiler->op_count;

    if (unless)
		emit_jump_if(compiler, reg, 0);
	else
		emit_jump_ifnot(compiler, reg, 0);

	// evaluate code
	compile_list(compiler, reg, code, false);

	// fixup jump
	fixup_jump(compiler, jump_op, compiler->op_count);
}

static void compile_if(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
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
	compile(compiler, reg, cond);
	
	// jump over if code
	unsigned int jump_ifnot_op = compiler->op_count;
	
	emit_jump_ifnot(compiler, reg, 0);
	
	// evaluate if code
	compile(compiler, reg, ifcode);
	
	// jump over else code
	unsigned int jump_op = compiler->op_count;
	
	emit_jump(compiler, 0);
	
	// else code
	compile(compiler, reg, elsecode);
	
	// fixup jumps
	fixup_jump(compiler, jump_ifnot_op, jump_op + 1);
	fixup_jump(compiler, jump_op, compiler->op_count);
}

static void compile_set(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "set!: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, car && car->type == LUSP_AST_SYMBOL, "set!: first argument has to be a symbol");
	check(compiler, cdr && cdr->type == LUSP_AST_CONS && cdr->cons.cdr == 0, "set!: malformed syntax");
	
	// evaluate value
	compile(compiler, reg, cdr->cons.car);
	
	// set value
    compile_symbol_getset(compiler, reg, car->symbol, true);
}

static void compile_let_pushvardecl(struct compiler_t* compiler, struct scope_t* scope, struct lusp_ast_node_t* vardecl)
{
    check(compiler, vardecl && vardecl->type == LUSP_AST_CONS, "let: malformed syntax");

    struct lusp_ast_node_t* var = vardecl->cons.car;
    struct lusp_ast_node_t* decl = vardecl->cons.cdr;

    check(compiler, var && var->type == LUSP_AST_SYMBOL, "let: malformed syntax");
    check(compiler, decl && decl->type == LUSP_AST_CONS && decl->cons.cdr == 0, "let: malformed syntax");

    // add value to new scope
    unsigned int index = allocate_registers(compiler, 1);
    
    check(compiler, !find_bind_local(scope, var->symbol), "let: duplicate arguments detected");
    
    scope->binds[scope->bind_count].symbol = var->symbol;
    scope->binds[scope->bind_count].index = index;
    scope->binds[scope->bind_count].has_upval = false;
    scope->bind_count++;

    // compute value in the old scope
    compile(compiler, index, decl->cons.car);
}

static void compile_letseq_helper(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
    // just compile the body if there are no arguments
	if (!args)
	{
	    compile_list(compiler, reg, body, false);
	    return;
	}
	
	// add new scope
	struct scope_t scope;
	scope.compiler = compiler;
	scope.bind_count = 0;
	
	// add first binding
	check(compiler, args && args->type == LUSP_AST_CONS, "let*: malformed syntax");
	
	compile_let_pushvardecl(compiler, &scope, args->cons.car);
	
	// evaluate the rest recursively in the new scope
	push_scope(compiler, &scope);
	compile_letseq_helper(compiler, reg, args->cons.cdr, body);
	pop_scope(compiler);
}

static void compile_letseq(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "let*: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	unsigned int free_reg = compiler->free_reg;
	
	compile_letseq_helper(compiler, reg, car, cdr);
	
	// free registers for new scope vars
	compiler->free_reg = free_reg;
}

static void compile_let(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "let: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, !car || car->type == LUSP_AST_CONS, "let: first argument has to be a list");
	
	// add new scope
	struct scope_t scope;
	scope.compiler = compiler;
	scope.bind_count = 0;
	
	// fill new scope with arguments and compute values
	unsigned int free_reg = compiler->free_reg;
	
	for (struct lusp_ast_node_t* vdlist = car; vdlist; vdlist = vdlist->cons.cdr)
	{
		check(compiler, vdlist->type == LUSP_AST_CONS, "let: malformed syntax");
		
		compile_let_pushvardecl(compiler, &scope, vdlist->cons.car);
	}
	
	// evaluate body in new scope
	push_scope(compiler, &scope);
	compile_list(compiler, reg, cdr, false);
	pop_scope(compiler);
	
	// free registers for new scope vars
	compiler->free_reg = free_reg;
}

static void compile_closure(struct compiler_t* parent, unsigned int reg, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
	struct compiler_t compiler;
	
	// compile closure
	struct lusp_vm_bytecode_t* bytecode = create_closure(&compiler, parent, args, body);
	
	// create closure
	emit_create_closure(parent, reg, bytecode);
	
	// set upvalues
	for (unsigned int i = 0; i < compiler.upval_count; ++i)
	{
		struct upval_t u = compiler.upvals[i];
		
		compile_bind_getset(parent, 0, u.scope, u.binding, false);
	}
}

static void compile_lambda(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "lambda: malformed syntax");
	
	compile_closure(compiler, reg, args->cons.car, args->cons.cdr);
}

static void compile_define(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS, "define: malformed syntax");
	
	struct lusp_ast_node_t* car = args->cons.car;
	struct lusp_ast_node_t* cdr = args->cons.cdr;
	
	check(compiler, car && (car->type == LUSP_AST_SYMBOL || car->type == LUSP_AST_CONS),
		"define: first argument has to be either symbol or list");
	check(compiler, car->type == LUSP_AST_SYMBOL || car->cons.car->type == LUSP_AST_SYMBOL,
		"define: function declaration has to start with symbol");
	
	// get actual symbol
	struct lusp_object_t symbol = (car->type == LUSP_AST_SYMBOL) ? car->symbol : car->cons.car->symbol;
	
	// compile closure/value
	if (car->type == LUSP_AST_CONS)
    	compile_closure(compiler, reg, car->cons.cdr, cdr);
	else
	{
		check(compiler, !cdr || (cdr->type == LUSP_AST_CONS && cdr->cons.cdr == 0), "define: malformed syntax");
		
		compile(compiler, reg, cdr ? cdr->cons.car : 0);
	}
	
	// set value to slot ($$$ support local defines)
	emit_loadstore_global(compiler, reg, lusp_environment_get_slot(compiler->env, symbol), true);
}

static struct lusp_object_t compile_quote_helper(struct compiler_t* compiler, struct lusp_ast_node_t* node)
{
	if (!node) return lusp_mknull();
	
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

static void compile_quote(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	check(compiler, args && args->type == LUSP_AST_CONS && args->cons.cdr == 0, "quote: malformed syntax");
	
	compile_object(compiler, reg, compile_quote_helper(compiler, args->cons.car));
}

static void compile_begin(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* args)
{
	compile_list(compiler, reg, args, false);
}

static void compile_cons(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* node)
{
	DL_ASSERT(node && node->type == LUSP_AST_CONS);
	
	struct lusp_ast_node_t* func = node->cons.car;
	struct lusp_ast_node_t* args = node->cons.cdr;
	
	check(compiler, func != 0, "invalid function call");
	
	switch (func->type)
	{
	case LUSP_AST_SYMBOL:
	case LUSP_AST_CONS:
		return compile_call(compiler, reg, func, args);
		
	case LUSP_AST_SYMBOL_QUOTE:
		return compile_quote(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_BEGIN:
		return compile_begin(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_DEFINE:
		return compile_define(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_LAMBDA:
		return compile_lambda(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_LET:
		return compile_let(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_LETSEQ:
		return compile_letseq(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_SET:
		return compile_set(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_IF:
		return compile_if(compiler, reg, args);
		
	case LUSP_AST_SYMBOL_WHEN:
		return compile_whenunless(compiler, reg, args, false);
		
	case LUSP_AST_SYMBOL_UNLESS:
		return compile_whenunless(compiler, reg, args, true);
		
	case LUSP_AST_SYMBOL_DO:
		return compile_do(compiler, reg, args);
		
	default:
		check(compiler, false, "invalid function call");
	}
}

static void compile(struct compiler_t* compiler, unsigned int reg, struct lusp_ast_node_t* node)
{
	if (!node) return compile_object(compiler, reg, lusp_mknull());
	
	switch (node->type)
	{
	case LUSP_AST_SYMBOL:
		return compile_symbol(compiler, reg, node);
	
	case LUSP_AST_LITERAL:
		return compile_literal(compiler, reg, node);
		
	case LUSP_AST_CONS:
		return compile_cons(compiler, reg, node);
 
	default:
		check(compiler, false, "unknown node type");
	}
}

static void compile_closure_code(struct compiler_t* compiler, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
	DL_ASSERT(compiler->free_reg == 0);
	
    // add top-level scope
    struct scope_t scope;
    scope.compiler = compiler;
    scope.bind_count = 0;

    // fill scope with arguments
    bool rest = false;
    
    for (struct lusp_ast_node_t* arg = args; arg; arg = arg->cons.cdr)
    {
        check(compiler, arg->type == LUSP_AST_CONS || arg->type == LUSP_AST_SYMBOL, "lambda: malformed syntax");
        check(compiler, arg->type == LUSP_AST_SYMBOL || (arg->cons.car && arg->cons.car->type == LUSP_AST_SYMBOL), "lambda: malformed syntax");
        
        rest = (arg->type == LUSP_AST_SYMBOL);
        
        struct lusp_object_t symbol = (arg->type == LUSP_AST_CONS) ? arg->cons.car->symbol : arg->symbol;
        
        check(compiler, !find_bind_local(&scope, symbol), "lambda: duplicate arguments detected");

        scope.binds[scope.bind_count].symbol = symbol;
        scope.binds[scope.bind_count].index = allocate_registers(compiler, 1);
        scope.binds[scope.bind_count].has_upval = false;
        scope.bind_count++;
        
        if (rest) break;
    }
    
    // create list for rest
    if (rest) emit_create_list(compiler, scope.bind_count - 1);
    
    // allocate register for return value
    unsigned int reg = allocate_registers(compiler, 1);
    
    // compile body
    push_scope(compiler, &scope);
    compile_list(compiler, reg, body, false);
    pop_scope(compiler);
    
    // return
    emit_return(compiler, reg);
}

static struct lusp_vm_bytecode_t* create_closure(struct compiler_t* compiler, struct compiler_t* parent, struct lusp_ast_node_t* args, struct lusp_ast_node_t* body)
{
    // create compiler
    compiler->env = parent->env;
    compiler->scope = parent->scope;
    compiler->free_reg = 0;
    compiler->reg_count = 0;
    compiler->upval_count = 0;
    compiler->op_count = 0;
    compiler->flags = parent->flags;
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
    code->reg_count = compiler->reg_count;
    code->upval_count = compiler->upval_count;
    code->ops = ops;
    code->op_count = compiler->op_count;
	code->jit = 0;
	
	lusp_bytecode_setup(code);
	
    return code;
}

struct lusp_object_t lusp_compile_ast(struct lusp_environment_t* env, struct lusp_ast_node_t* node, unsigned int flags)
{
	// setup error handling facilities
	jmp_buf buf;
	
	if (setjmp(buf)) return lusp_mknull();
	
	// create compiler 
	struct compiler_t parent;
	
    parent.env = env;
    parent.scope = 0;
    parent.flags = flags;
    parent.error = &buf;
    
    // compile bytecode
    struct compiler_t compiler;
    
	struct lusp_vm_bytecode_t* bytecode = create_closure(&compiler, &parent, 0, node);
	
	// check correctness
	DL_ASSERT(compiler.upval_count == 0);
	
	// create resulting closure
	return lusp_mkclosure(bytecode, 0);
}
