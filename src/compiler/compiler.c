// DeepLight Engine (c) Zeux 2006-2010

#include <core/common.h>

#include <lusp/compiler/compiler.h>

#include <lusp/object.h>
#include <lusp/environment.h>
#include <lusp/memory.h>
#include <lusp/compile.h>
#include <lusp/vm/bytecode.h>

#include <lusp/compiler/lexer.h>
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
		
	// mark scope
	scope->has_upvals = true;
	
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

static inline void pop_scope(struct compiler_t* compiler, unsigned int first_reg)
{
	struct scope_t* scope = compiler->scope;
	
	// close bindings
	if (scope->has_upvals) emit_close(compiler, first_reg);
	
	compiler->scope = scope->parent;
}

static inline unsigned int allocate_registers(struct compiler_t* compiler, unsigned int count)
{
	unsigned int result = compiler->free_reg;
	compiler->free_reg += count;
	compiler->reg_count = max(compiler->reg_count, compiler->free_reg);
	return result;
}

static inline struct binding_t* add_bind(struct compiler_t* compiler, struct scope_t* scope, struct lusp_object_t symbol)
{
	// allocate register
	unsigned int reg = allocate_registers(compiler, 1);
	
	// add binding
	struct binding_t* bind = &scope->binds[scope->bind_count++];
	
	bind->symbol = symbol;
	bind->index = reg;
	
	return bind;
}

static void create_compiler(struct compiler_t* compiler, struct compiler_t* parent)
{
	// create compiler
	compiler->env = parent->env;
	compiler->scope = parent->scope;
	compiler->free_reg = 0;
	compiler->reg_count = 0;
	compiler->upval_count = 0;
	compiler->op_count = 0;
	compiler->flags = parent->flags;
}

static struct lusp_vm_bytecode_t* create_closure(struct compiler_t* compiler)
{
	// create new closure
	unsigned int ops_size = compiler->op_count * sizeof(struct lusp_vm_op_t);

	struct lusp_vm_op_t* ops = (struct lusp_vm_op_t*)lusp_memory_allocate(ops_size);
	DL_ASSERT(ops);

	memcpy(ops, compiler->ops, ops_size);

	struct lusp_vm_bytecode_t* code = (struct lusp_vm_bytecode_t*)lusp_memory_allocate(sizeof(struct lusp_vm_bytecode_t));
	DL_ASSERT(code);

	code->env = compiler->env;
	code->reg_count = compiler->reg_count;
	code->upval_count = compiler->upval_count;
	code->ops = ops;
	code->op_count = compiler->op_count;

	lusp_setup_bytecode(code);

	return code;
}

static void compile_literal(struct compiler_t* compiler, unsigned int reg, struct lusp_object_t object)
{
	struct lusp_object_t o = lusp_mkcons(object, object);

	emit_load_const(compiler, reg, o.cons);
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

	struct scope_t* scope = 0;
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

static void compile_symbol(struct compiler_t* compiler, unsigned int reg, struct lusp_object_t symbol)
{
	compile_symbol_getset(compiler, reg, symbol, false);
}

static void compile_expr(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg);
static void compile_block(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg);

static void compile_literal_next(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg, struct lusp_object_t object)
{
	lusp_lexer_next(lexer);
	
	return compile_literal(compiler, reg, object);
}

static void compile_call(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg, struct lusp_object_t symbol)
{
	// skip open paren
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_OPEN_PAREN);
	lusp_lexer_next(lexer);
	
	unsigned int free_reg = compiler->free_reg;
	
	// allocate registers for call frame
	unsigned int frame_regs = allocate_registers(compiler, 2);
	unsigned int arg_regs = frame_regs + 2;
	unsigned int last_arg_reg = arg_regs - 1; // does not really mean anything for first argument
	
	while (lexer->lexeme != LUSP_LEXEME_CLOSE_PAREN)
	{
		// allocate register for new argument
		unsigned int arg_reg = allocate_registers(compiler, 1);
		DL_ASSERT(arg_reg == last_arg_reg + 1);
		last_arg_reg = arg_reg;
		
		// evaluate argument
		compile_expr(lexer, compiler, arg_reg);
		
		if (lexer->lexeme == LUSP_LEXEME_COMMA)
			CHECK(lusp_lexer_next(lexer) != LUSP_LEXEME_CLOSE_PAREN, "expected argument after comma");
		else
			CHECK(lexer->lexeme == LUSP_LEXEME_CLOSE_PAREN, "comma or closing paren expected in argument list");
	}
	
	// skip close paren
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_CLOSE_PAREN);
	lusp_lexer_next(lexer);
	
	// evaluate function
	compile_symbol(compiler, reg, symbol);
	
	// call function
	unsigned int arg_count = last_arg_reg + 1 - arg_regs;
	
	emit_call(compiler, reg, arg_regs, arg_count);
	
	// free temporary registers
	compiler->free_reg = free_reg;
}

static void compile_assign(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg, struct lusp_object_t symbol)
{
	// skip assign sign
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_ASSIGN);
	lusp_lexer_next(lexer);
	
	// evaluate expression
	compile_expr(lexer, compiler, reg);
	
	// assign
	compile_symbol_getset(compiler, reg, symbol, true);
}

static void compile_let(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// skip let
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_SYMBOL_LET);
	lusp_lexer_next(lexer);
	
	// read symbol
	CHECK(lexer->lexeme == LUSP_LEXEME_SYMBOL, "variable name expected after let");
	
	struct lusp_object_t symbol = lusp_mksymbol(lexer->value.symbol);
	lusp_lexer_next(lexer);
	
	// add variable to current scope
	CHECK(find_bind_local(compiler->scope, symbol) == 0, "%s: variable redefinition", symbol.symbol->name);
	
	struct binding_t* bind = add_bind(compiler, compiler->scope, symbol);
	
	// is it assigned right away?
	if (lexer->lexeme == LUSP_LEXEME_ASSIGN)
		compile_assign(lexer, compiler, reg, symbol);
	else
		emit_move(compiler, reg, bind->index);
}

static void compile_if(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// skip if
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_SYMBOL_IF);
	lusp_lexer_next(lexer);
	
	// evaluate condition
	compile_expr(lexer, compiler, reg);

	// jump over if code
	unsigned int jump_ifnot_op = compiler->op_count;

	emit_jump_ifnot(compiler, reg, 0);

	// evaluate if code
	compile_block(lexer, compiler, reg);
	
	if (lexer->lexeme == LUSP_LEXEME_SYMBOL_ELSE)
	{
		// skip else
		DL_ASSERT(lexer->lexeme == LUSP_LEXEME_SYMBOL_ELSE);
		lusp_lexer_next(lexer);
	
		// jump over else code
		unsigned int jump_op = compiler->op_count;

		emit_jump(compiler, 0);

		// evaluate else code
		compile_block(lexer, compiler, reg);

		// fixup jumps
		fixup_jump(compiler, jump_ifnot_op, jump_op + 1);
		fixup_jump(compiler, jump_op, compiler->op_count);
	}
	else
	{
		// fixup jumps
		fixup_jump(compiler, jump_ifnot_op, compiler->op_count);
	}
}

static void compile_list(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg, enum lusp_lexeme_t stop_lexeme)
{
	if (lexer->lexeme == stop_lexeme) compile_literal(compiler, reg, lusp_mknull());
	
	while (lexer->lexeme != stop_lexeme) compile_expr(lexer, compiler, reg);
}

static void compile_block(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	if (lexer->lexeme == LUSP_LEXEME_OPEN_BRACE)
	{
		// skip open brace
		DL_ASSERT(lexer->lexeme == LUSP_LEXEME_OPEN_BRACE);
		lusp_lexer_next(lexer);
		
		// compile block contents
		compile_list(lexer, compiler, reg, LUSP_LEXEME_CLOSE_BRACE);
		
		// skip close brace
		DL_ASSERT(lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE);
		lusp_lexer_next(lexer);
	}
	else compile_expr(lexer, compiler, reg);
}

static void compile_closure_body(struct lusp_lexer_t* lexer, struct compiler_t* compiler, bool global)
{
	// add new scope
	struct scope_t scope;
	scope.compiler = compiler;
	scope.bind_count = 0;
	scope.has_upvals = false;

	// remember last free register (should always be 0?)
	unsigned int free_reg = compiler->free_reg;
	
	if (!global)
	{
		// skip vertical bar
		DL_ASSERT(lexer->lexeme == LUSP_LEXEME_VERTICAL_BAR);
		lusp_lexer_next(lexer);
		
		// parse parameter list
		while (lexer->lexeme != LUSP_LEXEME_VERTICAL_BAR)
		{
			// read symbol
			CHECK(lexer->lexeme == LUSP_LEXEME_SYMBOL, "expected symbol");
			struct lusp_object_t symbol = lusp_mksymbol(lexer->value.symbol);
			
			lusp_lexer_next(lexer);
			
			// add binding
			struct binding_t* bind = add_bind(compiler, &scope, symbol);
			DL_ASSERT(bind->index + 1 == scope.bind_count);
			
			if (lexer->lexeme == LUSP_LEXEME_COMMA)
				CHECK(lusp_lexer_next(lexer) == LUSP_LEXEME_SYMBOL, "expected symbol after comma");
			else
				CHECK(lexer->lexeme == LUSP_LEXEME_VERTICAL_BAR, "comma or vertical bar expected in parameter list");
		}
		
		// skip vertical bar
		DL_ASSERT(lexer->lexeme == LUSP_LEXEME_VERTICAL_BAR);
		lusp_lexer_next(lexer);
	}

	// allocate register for return value
	unsigned int ret_reg = allocate_registers(compiler, 1);

	// evaluate body in new scope
	push_scope(compiler, &scope);
	global ? compile_list(lexer, compiler, ret_reg, LUSP_LEXEME_EOF) : compile_block(lexer, compiler, ret_reg);
	pop_scope(compiler, free_reg);

	emit_return(compiler, ret_reg);
}

static void compile_closure(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// create new compiler
	struct compiler_t child;
	
	create_compiler(&child, compiler);
	
	// compile closure
	compile_closure_body(lexer, &child, false);
	
	// create bytecode
	struct lusp_vm_bytecode_t* bytecode = create_closure(&child);
	
	// creeate closure
	emit_create_closure(compiler, reg, bytecode);
	
	// set upvalues
	for (unsigned int i = 0; i < child.upval_count; ++i)
	{
		struct upval_t u = child.upvals[i];

		compile_bind_getset(compiler, 0, u.scope, u.binding, false);
	}
}

static void compile_parens(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// skip open paren
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_OPEN_PAREN);
	lusp_lexer_next(lexer);
	
	// evaluate expression
	compile_expr(lexer, compiler, reg);
	
	// skip close paren
	CHECK(lexer->lexeme == LUSP_LEXEME_CLOSE_PAREN, "expected close paren");
	lusp_lexer_next(lexer);
}

static void compile_term(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	switch (lexer->lexeme)
	{
	case LUSP_LEXEME_LITERAL_BOOLEAN:
		return compile_literal_next(lexer, compiler, reg, lusp_mkboolean(lexer->value.boolean));
		
	case LUSP_LEXEME_LITERAL_INTEGER:
		return compile_literal_next(lexer, compiler, reg, lusp_mkinteger(lexer->value.integer));
		
	case LUSP_LEXEME_LITERAL_REAL:
		return compile_literal_next(lexer, compiler, reg, lusp_mkreal(lexer->value.real));
		
	case LUSP_LEXEME_LITERAL_STRING:
		return compile_literal_next(lexer, compiler, reg, lusp_mkstring(lexer->value.string));
		
	case LUSP_LEXEME_VERTICAL_BAR:
		return compile_closure(lexer, compiler, reg);
	
	case LUSP_LEXEME_OPEN_PAREN:
		return compile_parens(lexer, compiler, reg);
		
	case LUSP_LEXEME_SYMBOL:
	{
		struct lusp_object_t symbol = lusp_mksymbol(lexer->value.symbol);
		
		switch (lusp_lexer_next(lexer))
		{
		case LUSP_LEXEME_OPEN_PAREN:
			return compile_call(lexer, compiler, reg, symbol);
			
		case LUSP_LEXEME_ASSIGN:
			return compile_assign(lexer, compiler, reg, symbol);
			
		default:
			return compile_symbol(compiler, reg, symbol);
		}
	} break;
	
	default:
		CHECK(false, "expected term");
	}
}

static void compile_addexpr(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// evaluate left expression
	compile_term(lexer, compiler, reg);
	
	while (lexer->lexeme == LUSP_LEXEME_ADD || lexer->lexeme == LUSP_LEXEME_SUBTRACT)
	{
		// consume lexeme
		enum lusp_lexeme_t lexeme = lexer->lexeme;
		lusp_lexer_next(lexer);
		
		// allocate register
		unsigned int free_reg = compiler->free_reg;
		unsigned int temp_reg = allocate_registers(compiler, 1);
		
		// evaluate right expression
		compile_term(lexer, compiler, temp_reg);
		
		// evaluate relop
		switch (lexeme)
		{
		case LUSP_LEXEME_ADD:
			emit_binop(compiler, LUSP_VMOP_ADD, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_SUBTRACT:
			emit_binop(compiler, LUSP_VMOP_SUBTRACT, reg, reg, temp_reg);
			break;
			
		default:
			DL_ASSERT(false);
		}
		
		// free register
		compiler->free_reg = free_reg;
	}
}

static void compile_mulexpr(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// evaluate left expression
	compile_addexpr(lexer, compiler, reg);
	
	while (lexer->lexeme == LUSP_LEXEME_MULTIPLY || lexer->lexeme == LUSP_LEXEME_DIVIDE ||
		lexer->lexeme == LUSP_LEXEME_MODULO)
	{
		// consume lexeme
		enum lusp_lexeme_t lexeme = lexer->lexeme;
		lusp_lexer_next(lexer);
		
		// allocate register
		unsigned int free_reg = compiler->free_reg;
		unsigned int temp_reg = allocate_registers(compiler, 1);
		
		// evaluate right expression
		compile_addexpr(lexer, compiler, temp_reg);
		
		// evaluate relop
		switch (lexeme)
		{
		case LUSP_LEXEME_MULTIPLY:
			emit_binop(compiler, LUSP_VMOP_MULTIPLY, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_DIVIDE:
			emit_binop(compiler, LUSP_VMOP_DIVIDE, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_MODULO:
			emit_binop(compiler, LUSP_VMOP_MODULO, reg, reg, temp_reg);
			break;
			
		default:
			DL_ASSERT(false);
		}
		
		// free register
		compiler->free_reg = free_reg;
	}
}

static void compile_relexpr(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// evaluate left expression
	compile_mulexpr(lexer, compiler, reg);
	
	while (lexer->lexeme == LUSP_LEXEME_LESS || lexer->lexeme == LUSP_LEXEME_LESS_EQUAL ||
		lexer->lexeme == LUSP_LEXEME_GREATER || lexer->lexeme == LUSP_LEXEME_GREATER_EQUAL)
	{
		// consume lexeme
		enum lusp_lexeme_t lexeme = lexer->lexeme;
		lusp_lexer_next(lexer);
		
		// allocate register
		unsigned int free_reg = compiler->free_reg;
		unsigned int temp_reg = allocate_registers(compiler, 1);
		
		// evaluate right expression
		compile_mulexpr(lexer, compiler, temp_reg);
		
		// evaluate relop
		switch (lexeme)
		{
		case LUSP_LEXEME_LESS:
			emit_binop(compiler, LUSP_VMOP_LESS, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_LESS_EQUAL:
			emit_binop(compiler, LUSP_VMOP_LESS_EQUAL, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_GREATER:
			emit_binop(compiler, LUSP_VMOP_GREATER, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_GREATER_EQUAL:
			emit_binop(compiler, LUSP_VMOP_GREATER_EQUAL, reg, reg, temp_reg);
			break;
			
		default:
			DL_ASSERT(false);
		}
		
		// free register
		compiler->free_reg = free_reg;
	}
}

static void compile_eqexpr(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	// evaluate left expression
	compile_relexpr(lexer, compiler, reg);
	
	while (lexer->lexeme == LUSP_LEXEME_EQUAL || lexer->lexeme == LUSP_LEXEME_NOT_EQUAL)
	{
		// consume lexeme
		enum lusp_lexeme_t lexeme = lexer->lexeme;
		lusp_lexer_next(lexer);
		
		// allocate register
		unsigned int free_reg = compiler->free_reg;
		unsigned int temp_reg = allocate_registers(compiler, 1);
		
		// evaluate right expression
		compile_relexpr(lexer, compiler, temp_reg);
		
		// evaluate relop
		switch (lexeme)
		{
		case LUSP_LEXEME_EQUAL:
			emit_binop(compiler, LUSP_VMOP_EQUAL, reg, reg, temp_reg);
			break;
			
		case LUSP_LEXEME_NOT_EQUAL:
			emit_binop(compiler, LUSP_VMOP_NOT_EQUAL, reg, reg, temp_reg);
			break;
			
		default:
			DL_ASSERT(false);
		}
		
		// free register
		compiler->free_reg = free_reg;
	}
}

static void compile_expr(struct lusp_lexer_t* lexer, struct compiler_t* compiler, unsigned int reg)
{
	switch (lexer->lexeme)
	{
	case LUSP_LEXEME_SYMBOL_LET:
		return compile_let(lexer, compiler, reg);
		
	case LUSP_LEXEME_SYMBOL_IF:
		return compile_if(lexer, compiler, reg);
	
	default:
		return compile_eqexpr(lexer, compiler, reg);
	}
}

struct lusp_object_t lusp_compile_ex(struct lusp_environment_t* env, struct lusp_lexer_t* lexer, struct mem_arena_t* arena, unsigned int flags)
{
	(void)arena;
	
	// create fake parent compiler 
	struct compiler_t parent;
	
    parent.env = env;
    parent.scope = 0;
    parent.flags = flags;
    
    // create actual compiler
    struct compiler_t compiler;
    
    create_compiler(&compiler, &parent);
    
    // compile closure body
    compile_closure_body(lexer, &compiler, true);
    
    // create bytecode
	struct lusp_vm_bytecode_t* bytecode = create_closure(&compiler);
	
	// check correctness
	DL_ASSERT(compiler.upval_count == 0);
	
	// create resulting closure
	return lusp_mkclosure(bytecode, 0);
}