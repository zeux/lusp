// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_object_t;

enum lusp_vm_opcode_t
{
	LUSP_VMOP_GET_OBJECT,
	LUSP_VMOP_GET_LOCAL,
	LUSP_VMOP_SET_LOCAL,
	LUSP_VMOP_GET_GLOBAL,
	LUSP_VMOP_SET_GLOBAL,
	LUSP_VMOP_PUSH,
	LUSP_VMOP_BIND,
	LUSP_VMOP_UNBIND,
	LUSP_VMOP_CALL,
	LUSP_VMOP_RETURN,
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
			unsigned int index;
		} getset_global;
		
		struct
		{
			unsigned int count;
		} bind;
		
		struct
		{
            unsigned int count;
		} call;
	};
};

struct lusp_vm_environment_t
{
	struct lusp_vm_environment_t* parent;
	
	struct lusp_object_t** binds;
	unsigned int count;
};

struct lusp_vm_continuation_t
{
	struct lusp_vm_op_t* pc;
	struct lusp_vm_environment_t* envt;
	struct lusp_vm_continuation_t* cont;
};

struct lusp_vm_bytecode_t
{
	struct lusp_vm_op_t* ops;
	unsigned int count;
};