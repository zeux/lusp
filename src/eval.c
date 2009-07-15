// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm.h>
#include <lusp/object.h>
#include <lusp/write.h>

#include <stdio.h>

static void dump_bytecode(struct lusp_vm_bytecode_t* code)
{
	for (unsigned int i = 0; i < code->count; ++i)
	{
		struct lusp_vm_op_t* op = &code->ops[i];
		
		switch (op->opcode)
		{
		case LUSP_VMOP_GET_OBJECT:
			printf("%02d: get_object %p [ ", i, op->get_object.object);
			lusp_write(op->get_object.object);
			printf(" ]\n");
			break;
			
		case LUSP_VMOP_GET_LOCAL:
		case LUSP_VMOP_SET_LOCAL:
			printf("%02d: %s_local %d %d\n", i, (op->opcode == LUSP_VMOP_SET_LOCAL) ? "set" : "get",
				op->getset_local.depth, op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_GLOBAL:
		case LUSP_VMOP_SET_GLOBAL:
			printf("%02d: %s_global %d\n", i, (op->opcode == LUSP_VMOP_SET_GLOBAL) ? "set" : "get",
				op->getset_global.index);
			break;
			
		case LUSP_VMOP_PUSH:
			printf("%02d: push\n", i);
			break;
			
		case LUSP_VMOP_BIND:
			printf("%02d: bind %d\n", i, op->bind.count);
			break;
			
		case LUSP_VMOP_UNBIND:
			printf("%02d: unbind\n", i);
			break;
			
		case LUSP_VMOP_CALL:
			printf("%02d: call\n", i);
			break;
			
		case LUSP_VMOP_RETURN:
			printf("%02d: return\n", i);
			break;
			
		default:
			printf("%02d: unknown\n", i);
		}
	}
}

struct lusp_object_t* lusp_eval(struct lusp_object_t* object)
{
	if (!object || object->type != LUSP_OBJECT_CLOSURE) return 0;
	
	struct lusp_vm_bytecode_t* code = object->closure.code;
	
	dump_bytecode(code);

	return 0;
}