// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct compiler_t;

struct binding_t
{
	struct lusp_object_t symbol;
	unsigned int index;
};

struct scope_t
{
	struct compiler_t* compiler;
	
	struct scope_t* parent;
	
	struct binding_t binds[1024];
	unsigned int bind_count;
	
	bool has_upvals;
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
	
	// registers
	unsigned int free_reg;
	unsigned int reg_count;
	
	// upvalues
	struct upval_t upvals[1024];
	unsigned int upval_count;
	
	// opcode buffer
	struct lusp_vm_op_t ops[1024];
	unsigned int op_count;
	
	// compilation parameters
	unsigned int flags;
};

#define CHECK(condition, message, ...) do { if (!(condition)) lexer->error_handler(lexer, message, ## __VA_ARGS__); } while (0)