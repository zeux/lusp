// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#include <lusp/object.h>

struct lusp_environment_t;
struct lusp_environment_slot_t;
struct lusp_vm_bytecode_t;

enum lusp_vm_opcode_t
{
	LUSP_VMOP_GET_OBJECT,
	LUSP_VMOP_GET_LOCAL,
	LUSP_VMOP_SET_LOCAL,
	LUSP_VMOP_GET_UPVAL,
	LUSP_VMOP_SET_UPVAL,
	LUSP_VMOP_GET_GLOBAL,
	LUSP_VMOP_SET_GLOBAL,
	LUSP_VMOP_PUSH,
	LUSP_VMOP_CALL,
	LUSP_VMOP_RETURN,
	LUSP_VMOP_JUMP,
	LUSP_VMOP_JUMP_IF,
	LUSP_VMOP_JUMP_IFNOT,
	LUSP_VMOP_CREATE_CLOSURE,
	LUSP_VMOP_CREATE_LIST
};

struct lusp_vm_op_t
{
	enum lusp_vm_opcode_t opcode;
	
	union
	{
		struct
		{
			struct lusp_object_t* object;
		} get_object;
		
		struct
		{
			unsigned int index;
		} getset_local;
		
		struct
		{
			struct lusp_environment_slot_t* slot;
		} getset_global;
		
		struct
		{
            unsigned int count;
		} call;
		
		struct
		{
		    int offset;
		} jump;
		
		struct
		{
		    struct lusp_vm_bytecode_t* code;
		} create_closure;
		
		struct
		{
		    unsigned int index;
		} create_list;
	};
};

struct lusp_vm_upval_t
{
    struct lusp_object_t* ref;
    
    union
    {
        // for closed upval
        struct lusp_object_t object;
        
        // for open upval
        struct lusp_vm_upval_t* next;
    };
};

struct lusp_vm_closure_t
{
	struct lusp_vm_upval_t* upvals[1];
};

struct lusp_vm_bytecode_t;

typedef struct lusp_object_t (*lusp_vm_evaluator_t)(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* eval_stack, unsigned int arg_count);

struct lusp_vm_bytecode_t
{
    struct lusp_environment_t* env;
    
    unsigned int local_count;
    unsigned int upval_count;

	struct lusp_vm_op_t* ops;
	unsigned int op_count;

	void* jit;
	
	lusp_vm_evaluator_t evaluator;
};

void lusp_dump_bytecode(struct lusp_vm_bytecode_t* code, bool deep);
