// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/object.h>
#include <lusp/evalutils.h>

static struct lusp_object_t eval(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* eval_stack, unsigned int arg_count)
{
    struct lusp_vm_op_t* pc = code->ops;
    
    struct lusp_object_t value = lusp_mknull();
    
    struct lusp_object_t* eval_stack_top = eval_stack + code->local_count;
    
    struct lusp_vm_upval_t* upvals = 0;
    
    for (;;)
    {
		struct lusp_vm_op_t op = *pc++;
		
        switch (op.opcode)
        {
        case LUSP_VMOP_GET_OBJECT:
            value = *op.get_object.object;
            break;
            
        case LUSP_VMOP_GET_LOCAL:
			value = eval_stack[op.getset_local.index];
	        break;
            
        case LUSP_VMOP_SET_LOCAL:
			eval_stack[op.getset_local.index] = value;
	        break;
            
        case LUSP_VMOP_GET_UPVAL:
			value = *closure->upvals[op.getset_local.index]->ref;
			break;
			
        case LUSP_VMOP_SET_UPVAL:
			*closure->upvals[op.getset_local.index]->ref = value;
			break;
			
        case LUSP_VMOP_GET_GLOBAL:
            value = op.getset_global.slot->value;
            break;
            
        case LUSP_VMOP_SET_GLOBAL:
            op.getset_global.slot->value = value;
            break;
            
        case LUSP_VMOP_PUSH:
            *eval_stack_top++ = value;
            break;
            
        case LUSP_VMOP_CALL:
            DL_ASSERT(value.type == LUSP_OBJECT_CLOSURE || value.type == LUSP_OBJECT_FUNCTION);
            
            if (value.type == LUSP_OBJECT_CLOSURE)
            {
                unsigned int count = op.call.count;
                
                eval_stack_top -= count;
                
                struct lusp_vm_bytecode_t* code = value.closure->code;
                
				value = code->evaluator(code, value.closure, eval_stack_top, count);
            }
            else
            {
                unsigned int count = op.call.count;
                
                eval_stack_top -= count;
                value = ((lusp_function_t)value.function)(code->env, eval_stack_top, count);
            }
            break;
            
        case LUSP_VMOP_RETURN:
            close_upvals(upvals);
            return value;
            
        case LUSP_VMOP_JUMP:
            pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_JUMP_IF:
            if (value.type != LUSP_OBJECT_BOOLEAN || value.boolean) pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_JUMP_IFNOT:
            if (value.type == LUSP_OBJECT_BOOLEAN && !value.boolean) pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_CREATE_CLOSURE:
        {
            unsigned int upval_count = op.create_closure.code->upval_count;
            
            value = lusp_mkclosure(op.create_closure.code, upval_count);
            
            struct lusp_vm_closure_t* newclosure = value.closure;
            
            // set upvalues
            for (unsigned int i = 0; i < upval_count; ++i)
            {
				struct lusp_vm_op_t op = *pc++;
				
				switch (op.opcode)
				{
				case LUSP_VMOP_GET_LOCAL:
				    newclosure->upvals[i] = mkupval(&upvals, &eval_stack[op.getset_local.index]);
				    break;
				    
				case LUSP_VMOP_GET_UPVAL:
				    newclosure->upvals[i] = closure->upvals[op.getset_local.index];
				    break;
				    
				default:
				    DL_ASSERT(!"unexpected instruction");
				}
            }
        } break;
        
        case LUSP_VMOP_CREATE_LIST:
            eval_stack[op.create_list.index] = create_list(eval_stack + op.create_list.index, eval_stack + arg_count);
            break;
            
		default:
		    DL_ASSERT(!"unexpected instruction");
        }
    }
}

struct lusp_object_t lusp_eval(struct lusp_object_t object)
{
	if (object.type != LUSP_OBJECT_CLOSURE) return lusp_mknull();
	
	struct lusp_object_t eval_stack[1024];
	
	struct lusp_vm_bytecode_t* code = object.closure->code;
	
    return code->evaluator(code, 0, eval_stack, 0);
}

void lusp_bytecode_setup(struct lusp_vm_bytecode_t* code)
{
	code->evaluator = eval;
}
