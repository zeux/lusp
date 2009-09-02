// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/environment.h>
#include <lusp/object.h>

#include <lusp/vm/bytecode.h>
#include <lusp/vm/utils.h>

static struct lusp_vm_upval_t g_dummy_upval = {0, {{LUSP_OBJECT_NULL, {0}}}};

struct lusp_object_t lusp_eval_vm(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count)
{
    struct lusp_vm_op_t* pc = code->ops;
    struct lusp_vm_upval_t* upvals = &g_dummy_upval;

    for (;;)
    {
		struct lusp_vm_op_t op = *pc++;
		
        switch (op.opcode)
        {
        case LUSP_VMOP_LOAD_CONST:
            regs[op.reg] = *op.load_const.object;
            break;
            
        case LUSP_VMOP_LOAD_GLOBAL:
			regs[op.reg] = op.loadstore_global.slot->value;
	        break;
	        
        case LUSP_VMOP_STORE_GLOBAL:
			op.loadstore_global.slot->value = regs[op.reg];
	        break;
            
        case LUSP_VMOP_LOAD_UPVAL:
			regs[op.reg] = *closure->upvals[op.loadstore_upval.index]->ref;
	        break;
	        
        case LUSP_VMOP_STORE_UPVAL:
			*closure->upvals[op.loadstore_upval.index]->ref = regs[op.reg];
	        break;
            
        case LUSP_VMOP_MOVE:
			regs[op.reg] = regs[op.move.index];
            break;
            
        case LUSP_VMOP_CALL:
        {
			struct lusp_object_t func = regs[op.reg];
			struct lusp_object_t* args = regs + op.call.args;
			unsigned int count = op.call.count;
			
            DL_ASSERT(func.type == LUSP_OBJECT_CLOSURE || func.type == LUSP_OBJECT_FUNCTION);
            
            if (func.type == LUSP_OBJECT_CLOSURE)
            {
                // store call frame
                args[-2].type = LUSP_OBJECT_CALL_FRAME;
				struct lusp_vm_call_frame_t* frame = (struct lusp_vm_call_frame_t*)args[-2].call_frame;
				
				frame->closure = closure;
				frame->pc = pc;
				frame->regs = regs;
				
				// transfer control
				regs = args;
				closure = func.closure;
				pc = closure->code->ops;
				arg_count = count;
            }
            else
            {
                regs[op.reg] = ((lusp_function_t)func.function)(code->env, args, count);
            }
        } break;
            
        case LUSP_VMOP_RETURN:
        {
			DL_ASSERT(regs[-2].type == LUSP_OBJECT_CALL_FRAME);
			
			struct lusp_vm_call_frame_t* frame = (struct lusp_vm_call_frame_t*)regs[-2].call_frame;
			struct lusp_object_t* result = regs + op.reg;
			
			// top-level return
			if (frame->regs == 0) return *result;
			
			// restore regs
			regs = frame->regs;
			closure = frame->closure;
			pc = frame->pc;
			
			// store result
			DL_ASSERT((pc - 1)->opcode == LUSP_VMOP_CALL);
			regs[(pc - 1)->reg] = *result;
        } break;
            
        case LUSP_VMOP_JUMP:
            pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_JUMP_IF:
            if (regs[op.reg].type != LUSP_OBJECT_BOOLEAN || regs[op.reg].boolean) pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_JUMP_IFNOT:
            if (regs[op.reg].type == LUSP_OBJECT_BOOLEAN && !regs[op.reg].boolean) pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_CREATE_CLOSURE:
        {
            unsigned int upval_count = op.create_closure.code->upval_count;
            
            regs[op.reg] = lusp_mkclosure(op.create_closure.code, upval_count);
            
            struct lusp_vm_closure_t* newclosure = regs[op.reg].closure;
            
            // set upvalues
            for (unsigned int i = 0; i < upval_count; ++i)
            {
				struct lusp_vm_op_t op = *pc++;
				
				switch (op.opcode)
				{
				case LUSP_VMOP_MOVE:
				    newclosure->upvals[i] = mkupval(&upvals, &regs[op.move.index]);
				    break;
				    
				case LUSP_VMOP_LOAD_UPVAL:
				    newclosure->upvals[i] = closure->upvals[op.loadstore_upval.index];
				    break;
				    
				default:
				    DL_ASSERT(!"unexpected instruction");
				}
            }
        } break;
        
        case LUSP_VMOP_CLOSE:
			upvals = close_upvals(upvals, regs + op.close.begin);
			break;
            
		default:
		    DL_ASSERT(!"unexpected instruction");
        }
    }
}