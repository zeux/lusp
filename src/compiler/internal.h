// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#include <setjmp.h>
#include <stdio.h>

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
	
	// temp variables
	unsigned int current_temp_count;
	unsigned int temp_count;
	
	// upvalues
	struct upval_t upvals[1024];
	unsigned int upval_count;
	
	// opcode buffer
	struct lusp_vm_op_t ops[1024];
	unsigned int op_count;
	
	// compilation parameters
	unsigned int flags;
	
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
