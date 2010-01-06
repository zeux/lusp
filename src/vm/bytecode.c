// DeepLight Engine (c) Zeux 2006-2010

#include <core/common.h>

#include <lusp/vm/bytecode.h>

#include <lusp/write.h>
#include <lusp/environment.h>
#include <lusp/object.h>

#include <core/string.h>

#include <stdio.h>

static void dump_bytecode(struct lusp_vm_bytecode_t* code, const char* indent, bool deep)
{
	printf("%s%d registers, %d upvals\n", indent, code->reg_count, code->upval_count);
	
	for (unsigned int i = 0; i < code->op_count; ++i)
	{
		struct lusp_vm_op_t* op = &code->ops[i];
		
		printf("%s%02d: ", indent, i);
		
		switch (op->opcode)
		{
		case LUSP_VMOP_LOAD_CONST:
			printf("load_const r%d, %p [ ", op->reg, op->load_const.object);
			lusp_write(*op->load_const.object);
			printf(" ]\n");
			break;
			
		case LUSP_VMOP_LOAD_GLOBAL:
			printf("load_global r%d, %p [ %s ]\n", op->reg, op->loadstore_global.slot, op->loadstore_global.slot->name->name);
			break;
			
		case LUSP_VMOP_STORE_GLOBAL:
			printf("store_global %p [ %s ], r%d\n", op->loadstore_global.slot, op->loadstore_global.slot->name->name, op->reg);
			break;
			
		case LUSP_VMOP_LOAD_UPVAL:
			printf("load_upval r%d, u%d\n", op->reg, op->loadstore_upval.index);
			break;
			
		case LUSP_VMOP_STORE_UPVAL:
			printf("store_upval u%d, r%d\n", op->loadstore_upval.index, op->reg);
			break;
			
		case LUSP_VMOP_MOVE:
			printf("move r%d, r%d\n", op->reg, op->move.index);
			break;
			
		case LUSP_VMOP_CALL:
			printf("call r%d, r%d, %d\n", op->reg, op->call.args, op->call.count);
			break;
			
		case LUSP_VMOP_RETURN:
			printf("return r%d\n", op->reg);
			break;
			
		case LUSP_VMOP_JUMP:
			printf("jump %+d\n", op->jump.offset);
			break;
			
		case LUSP_VMOP_JUMP_IF:
			printf("jump_if r%d, %+d\n", op->reg, op->jump.offset);
			break;
			
		case LUSP_VMOP_JUMP_IFNOT:
			printf("jump_ifnot r%d, %+d\n", op->reg, op->jump.offset);
			break;
			
		case LUSP_VMOP_CREATE_CLOSURE:
			printf("create_closure r%d, %p\n", op->reg, op->create_closure.code);
			
			if (deep)
			{
    		    char new_indent[256];
    		    
    		    str_copy(new_indent, sizeof(new_indent), indent);
    		    str_concat(new_indent, sizeof(new_indent), "\t");
    		    
    		    dump_bytecode(op->create_closure.code, new_indent, deep);
		    }
    		break;
			
		case LUSP_VMOP_CLOSE:
			printf("close r%d\n", op->close.begin);
			break;
			
		case LUSP_VMOP_ADD:
			printf("add r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_SUBTRACT:
			printf("subtract r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_MULTIPLY:
			printf("multiply r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_DIVIDE:
			printf("divide r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_MODULO:
			printf("modulo r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_EQUAL:
			printf("equal r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_NOT_EQUAL:
			printf("not_equal r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_LESS:
			printf("less r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_LESS_EQUAL:
			printf("less_equal r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_GREATER:
			printf("greater r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
			break;
			
		case LUSP_VMOP_GREATER_EQUAL:
			printf("greater_equal r%d, r%d, r%d\n", op->reg, op->binop.left, op->binop.right);
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

void lusp_setup_bytecode(struct lusp_vm_bytecode_t* code)
{
#if DL_WINDOWS
    struct lusp_object_t lusp_eval_jit_x86_stub(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count);
    
    code->jit = lusp_eval_jit_x86_stub;
#else
    code->jit = 0;
#endif
}
