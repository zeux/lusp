// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#include <lusp/object.h>

struct lusp_environment_t;
struct lusp_environment_slot_t;
struct lusp_vm_bytecode_t;

enum lusp_vm_opcode_t
{
	LUSP_VMOP_LOAD_CONST,
	LUSP_VMOP_LOAD_GLOBAL,
	LUSP_VMOP_STORE_GLOBAL,
	LUSP_VMOP_LOAD_UPVAL,
	LUSP_VMOP_STORE_UPVAL,
	LUSP_VMOP_MOVE,
	LUSP_VMOP_CALL,
	LUSP_VMOP_RETURN,
	LUSP_VMOP_JUMP,
	LUSP_VMOP_JUMP_IF,
	LUSP_VMOP_JUMP_IFNOT,
	LUSP_VMOP_CREATE_CLOSURE,
	LUSP_VMOP_CREATE_LIST,
	LUSP_VMOP_CLOSE
};

struct lusp_vm_op_t
{
	uint8_t opcode;
	uint8_t padding;
	uint16_t reg;
	
	union
	{
		struct
		{
			struct lusp_object_t* object;
		} load_const;
		
		struct
		{
			struct lusp_environment_slot_t* slot;
		} loadstore_global;
		
		struct
		{
			unsigned int index;
		} loadstore_upval;
		
		struct
		{
			unsigned int index;
		} move;
		
		struct
		{
			uint16_t args;
			uint16_t count;
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
			unsigned int begin;
		} close;
		
		uint32_t dummy;
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

struct lusp_vm_bytecode_t;

struct lusp_vm_closure_t
{
	struct lusp_vm_bytecode_t* code;
	struct lusp_vm_upval_t* upvals[1];
};

struct lusp_vm_call_frame_t
{
	struct lusp_object_t* regs;
	struct lusp_vm_closure_t* closure;
	struct lusp_vm_op_t* pc;
};

typedef struct lusp_object_t (*lusp_vm_evaluator_t)(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count);

struct lusp_vm_bytecode_t
{
    struct lusp_environment_t* env;
    
    unsigned int reg_count;
    unsigned int upval_count;

	struct lusp_vm_op_t* ops;
	unsigned int op_count;

	void* jit;
};

void lusp_dump_bytecode(struct lusp_vm_bytecode_t* code, bool deep);
void lusp_setup_bytecode(struct lusp_vm_bytecode_t* code);
