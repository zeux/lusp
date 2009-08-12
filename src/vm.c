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
	printf("%s%d locals, %d uprefs\n", indent, code->local_count, code->upref_count);
	
	for (unsigned int i = 0; i < code->op_count; ++i)
	{
		struct lusp_vm_op_t* op = &code->ops[i];
		
		switch (op->opcode)
		{
		case LUSP_VMOP_GET_OBJECT:
			printf("%s%02d: get_object %p [ ", indent, i, op->get_object.object);
			lusp_write(op->get_object.object);
			printf(" ]\n");
			break;
			
		case LUSP_VMOP_GET_LOCAL:
		case LUSP_VMOP_SET_LOCAL:
			printf("%s%02d: %s_local %d\n", indent, i, (op->opcode == LUSP_VMOP_SET_LOCAL) ? "set" : "get", op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_UPVAL:
			printf("%s%02d: get_upval %d\n", indent, i, op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_UPREF:
		case LUSP_VMOP_SET_UPREF:
			printf("%s%02d: %s_upref %d\n", indent, i, (op->opcode == LUSP_VMOP_SET_UPREF) ? "set" : "get", op->getset_local.index);
			break;
			
		case LUSP_VMOP_GET_GLOBAL:
		case LUSP_VMOP_SET_GLOBAL:
			printf("%s%02d: %s_global %p [ %s ]\n", indent, i, (op->opcode == LUSP_VMOP_SET_GLOBAL) ? "set" : "get",
				op->getset_global.slot, op->getset_global.slot->name->symbol.name);
			break;
			
		case LUSP_VMOP_PUSH:
			printf("%s%02d: push\n", indent, i);
			break;
			
		case LUSP_VMOP_CALL:
			printf("%s%02d: call\n", indent, i);
			break;
			
		case LUSP_VMOP_RETURN:
			printf("%s%02d: return\n", indent, i);
			break;
			
		case LUSP_VMOP_JUMP:
			printf("%s%02d: jump %+d\n", indent, i, op->jump.offset);
			break;
			
		case LUSP_VMOP_JUMP_IF:
			printf("%s%02d: jump_if %+d\n", indent, i, op->jump.offset);
			break;
			
		case LUSP_VMOP_JUMP_IFNOT:
			printf("%s%02d: jump_ifnot %+d\n", indent, i, op->jump.offset);
			break;
			
		case LUSP_VMOP_CREATE_CLOSURE:
		{
			printf("%s%02d: create_closure %p\n", indent, i, op->create_closure.code);
			
		    char new_indent[256];
		    
		    str_copy(new_indent, sizeof(new_indent), indent);
		    str_concat(new_indent, sizeof(new_indent), "\t");
		    
		    dump_bytecode(op->create_closure.code, new_indent, deep);
		} break;
			
		default:
			printf("%s%02d: unknown\n", indent, i);
		}
	}
}

void lusp_dump_bytecode(struct lusp_vm_bytecode_t* code, bool deep)
{
    dump_bytecode(code, "", deep);
}
