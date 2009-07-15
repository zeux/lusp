// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm.h>
#include <lusp/object.h>
#include <lusp/write.h>
#include <lusp/environment.h>

#include <core/string.h>

#include <stdio.h>

static void dump_bytecode(struct lusp_vm_bytecode_t* code, const char* indent, bool deep)
{
	for (unsigned int i = 0; i < code->count; ++i)
	{
		struct lusp_vm_op_t* op = &code->ops[i];
		
		switch (op->opcode)
		{
		case LUSP_VMOP_GET_OBJECT:
			printf("%s%02d: get_object %p [ ", indent, i, op->get_object.object);
			lusp_write(op->get_object.object);
			printf(" ]\n");
			
			if (op->get_object.object && op->get_object.object->type == LUSP_OBJECT_CLOSURE && deep)
			{
			    char new_indent[256];
			    
			    str_copy(new_indent, sizeof(new_indent), indent);
			    str_concat(new_indent, sizeof(new_indent), "\t");
			    
			    dump_bytecode(op->get_object.object->closure.code, new_indent, deep);
			}
			
			break;
			
		case LUSP_VMOP_GET_LOCAL:
		case LUSP_VMOP_SET_LOCAL:
			printf("%s%02d: %s_local %d %d\n", indent, i, (op->opcode == LUSP_VMOP_SET_LOCAL) ? "set" : "get",
				op->getset_local.depth, op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_GLOBAL:
		case LUSP_VMOP_SET_GLOBAL:
			printf("%s%02d: %s_global %p [ %s ]\n", indent, i, (op->opcode == LUSP_VMOP_SET_GLOBAL) ? "set" : "get",
				op->getset_global.slot, op->getset_global.slot->name);
			break;
			
		case LUSP_VMOP_PUSH:
			printf("%s%02d: push\n", indent, i);
			break;
			
		case LUSP_VMOP_BIND:
			printf("%s%02d: bind %d\n", indent, i, op->bind.count);
			break;
			
		case LUSP_VMOP_UNBIND:
			printf("%s%02d: unbind\n", indent, i);
			break;
			
		case LUSP_VMOP_CALL:
			printf("%s%02d: call\n", indent, i);
			break;
			
		case LUSP_VMOP_RETURN:
			printf("%s%02d: return\n", indent, i);
			break;
			
		default:
			printf("%s%02d: unknown\n", indent, i);
		}
	}
}

struct lusp_object_t* lusp_eval(struct lusp_object_t* object)
{
	if (!object || object->type != LUSP_OBJECT_CLOSURE) return 0;
	
	struct lusp_vm_bytecode_t* code = object->closure.code;
	
	dump_bytecode(code, "", true);

	return 0;
}