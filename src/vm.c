// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/vm.h>

#include <lusp/write.h>
#include <lusp/environment.h>
#include <lusp/object.h>

#include <core/string.h>

#include <stdio.h>

static void dump_bytecode(struct lusp_vm_bytecode_t* code, const char* indent, bool deep)
{
	printf("%s%d locals, %d temps, %d upvals\n", indent, code->local_count, code->temp_count, code->upval_count);
	
	for (unsigned int i = 0; i < code->op_count; ++i)
	{
		struct lusp_vm_op_t* op = &code->ops[i];
		
		printf("%s%02d: ", indent, i);
		
		switch (op->opcode)
		{
		case LUSP_VMOP_GET_OBJECT:
			printf("get_object %p [ ", op->get_object.object);
			lusp_write(*op->get_object.object);
			printf(" ]\n");
			break;
			
		case LUSP_VMOP_GET_LOCAL:
		case LUSP_VMOP_SET_LOCAL:
			printf("%s_local %d\n", (op->opcode == LUSP_VMOP_SET_LOCAL) ? "set" : "get", op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_UPVAL:
		case LUSP_VMOP_SET_UPVAL:
			printf("%s_upval %d\n", (op->opcode == LUSP_VMOP_SET_UPVAL) ? "set" : "get", op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_GLOBAL:
		case LUSP_VMOP_SET_GLOBAL:
			printf("%s_global %p [ %s ]\n", (op->opcode == LUSP_VMOP_SET_GLOBAL) ? "set" : "get",
				op->getset_global.slot, op->getset_global.slot->name->name);
			break;
			
		case LUSP_VMOP_PUSH:
			printf("push\n");
			break;
			
		case LUSP_VMOP_CALL:
			printf("call %d\n", op->call.count);
			break;
			
		case LUSP_VMOP_RETURN:
			printf("return\n");
			break;
			
		case LUSP_VMOP_JUMP:
			printf("jump %+d\n", op->jump.offset);
			break;
			
		case LUSP_VMOP_JUMP_IF:
			printf("jump_if %+d\n", op->jump.offset);
			break;
			
		case LUSP_VMOP_JUMP_IFNOT:
			printf("jump_ifnot %+d\n", op->jump.offset);
			break;
			
		case LUSP_VMOP_CREATE_CLOSURE:
			printf("create_closure %p\n", op->create_closure.code);
			
			if (deep)
			{
    		    char new_indent[256];
    		    
    		    str_copy(new_indent, sizeof(new_indent), indent);
    		    str_concat(new_indent, sizeof(new_indent), "\t");
    		    
    		    dump_bytecode(op->create_closure.code, new_indent, deep);
		    }
    		break;
			
		case LUSP_VMOP_CREATE_LIST:
			printf("create_list %d\n", op->create_list.index);
			break;
			
		default:
			printf("unknown\n");
		}
	}
}

void lusp_dump_bytecode(struct lusp_vm_bytecode_t* code, bool deep)
{
    dump_bytecode(code, "", deep);
}
