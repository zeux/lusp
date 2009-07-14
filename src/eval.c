// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm.h>
#include <lusp/object.h>
#include <lusp/write.h>

#include <stdio.h>

struct lusp_object_t* lusp_eval(struct lusp_object_t* object)
{
	DL_ASSERT(object && object->type == LUSP_OBJECT_CLOSURE);
	
	struct lusp_vm_bytecode_t* code = object->closure.code;
	
	for (unsigned int i = 0; i < code->count; ++i)
	{
		struct lusp_vm_op_t* op = &code->ops[i];
		
		switch (op->opcode)
		{
		case LUSP_VMOP_LOAD_OBJECT:
			printf("%02d: load_object %p [ ", i, op->load_object.object);
			lusp_write(op->load_object.object);
			printf(" ]\n");
			break;
			
		case LUSP_VMOP_LOAD_LOCAL:
			printf("%02d: load_local %d %d\n", i, op->load_local.depth, op->load_local.index);
			break;
			
		case LUSP_VMOP_LOAD_GLOBAL:
			printf("%02d: load_global %d\n", i, op->load_global.index);
			break;
			
		case LUSP_VMOP_PUSH:
			printf("%02d: push\n", i);
			break;
			
		case LUSP_VMOP_BIND:
			printf("%02d: bind %d\n", i, op->bind.count);
			break;
			
		case LUSP_VMOP_PUSH_CONTINUATION:
			printf("%02d: push_continuation\n", i);
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

	return 0;
}
