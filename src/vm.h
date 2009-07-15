// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_object_t;
struct lusp_environment_t;
struct lusp_environment_slot_t;
struct lusp_vm_bytecode_t;

enum lusp_vm_opcode_t
{
	LUSP_VMOP_GET_OBJECT,
	LUSP_VMOP_GET_LOCAL,
	LUSP_VMOP_SET_LOCAL,
	LUSP_VMOP_GET_GLOBAL,
	LUSP_VMOP_SET_GLOBAL,
	LUSP_VMOP_PUSH,
	LUSP_VMOP_BIND,
	LUSP_VMOP_BIND_REST,
	LUSP_VMOP_UNBIND,
	LUSP_VMOP_CALL,
	LUSP_VMOP_RETURN,
	LUSP_VMOP_JUMP,
	LUSP_VMOP_JUMP_IFNOT,
	LUSP_VMOP_CREATE_CLOSURE
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
			unsigned int depth;
			unsigned int index;
		} getset_local;
		
		struct
		{
			struct lusp_environment_slot_t* slot;
		} getset_global;
		
		struct
		{
			unsigned int count;
		} bind;
		
		struct
		{
            unsigned int count;
		} call;
		
		struct
		{
		    unsigned int index;
		} jump;
		
		struct
		{
		    struct lusp_vm_bytecode_t* code;
		} create_closure;
	};
};

struct lusp_vm_bind_frame_t
{
	struct lusp_vm_bind_frame_t* parent;
	
	struct lusp_object_t** binds;
	unsigned int count;
};

struct lusp_vm_bytecode_t
{
    struct lusp_environment_t* env;

	struct lusp_vm_op_t* ops;
	unsigned int count;
};

void lusp_dump_bytecode(struct lusp_vm_bytecode_t* code, bool deep);
