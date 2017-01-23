#include "environment.h"
#include "object.h"

#include "bytecode.h"
#include "utils.h"

static struct lusp_vm_upval_t g_dummy_upval = {0, {{LUSP_OBJECT_NULL, {0}}}};

struct lusp_object_t lusp_eval_vm(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count)
{
	(void)arg_count;

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
			
            assert(func.type == LUSP_OBJECT_CLOSURE || func.type == LUSP_OBJECT_FUNCTION);
            
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
			assert(regs[-2].type == LUSP_OBJECT_CALL_FRAME);
			
			struct lusp_vm_call_frame_t* frame = (struct lusp_vm_call_frame_t*)regs[-2].call_frame;
			struct lusp_object_t* result = regs + op.reg;
			
			// top-level return
			if (frame->regs == 0) return *result;
			
			// restore regs
			regs = frame->regs;
			closure = frame->closure;
			pc = frame->pc;
			
			// store result
			assert((pc - 1)->opcode == LUSP_VMOP_CALL);
			regs[(pc - 1)->reg] = *result;
        } break;
            
        case LUSP_VMOP_JUMP:
            pc += op.jump.offset;
            break;
            
        case LUSP_VMOP_JUMP_IF:
			if (regs[op.reg].type != LUSP_OBJECT_BOOLEAN || regs[op.reg].boolean) pc += op.jump.offset;
            break;
            
        case LUSP_VMOP_JUMP_IFNOT:
            if (regs[op.reg].type == LUSP_OBJECT_BOOLEAN && !regs[op.reg].boolean) pc += op.jump.offset;
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
				    assert(!"unexpected instruction");
				}
            }
        } break;
        
        case LUSP_VMOP_CLOSE:
			upvals = close_upvals(upvals, regs + op.close.begin);
			break;
			
	#define BINOP(opcode, func) case opcode: regs[op.reg] = func(regs + op.binop.left, regs + op.binop.right); break
	
		BINOP(LUSP_VMOP_ADD, binop_add);
		BINOP(LUSP_VMOP_SUBTRACT, binop_subtract);
		BINOP(LUSP_VMOP_MULTIPLY, binop_multiply);
		BINOP(LUSP_VMOP_DIVIDE, binop_divide);
		BINOP(LUSP_VMOP_MODULO, binop_modulo);
		BINOP(LUSP_VMOP_EQUAL, binop_equal);
		BINOP(LUSP_VMOP_NOT_EQUAL, binop_not_equal);
		BINOP(LUSP_VMOP_LESS, binop_less);
		BINOP(LUSP_VMOP_LESS_EQUAL, binop_less_equal);
		BINOP(LUSP_VMOP_GREATER, binop_greater);
		BINOP(LUSP_VMOP_GREATER_EQUAL, binop_greater_equal);
		
	#undef BINOP
            
		default:
		    assert(!"unexpected instruction");
        }
    }
}
