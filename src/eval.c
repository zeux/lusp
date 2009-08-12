// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/object.h>
#include <lusp/memory.h>

extern struct mem_arena_t g_lusp_heap;

extern struct lusp_object_t g_lusp_false;

static inline struct lusp_vm_upval_t* mkupval(struct lusp_vm_upval_t** list, struct lusp_object_t** ref)
{
    // look for ref in list
    for (struct lusp_vm_upval_t* upval = *list; upval; upval = upval->next)
        if (upval->ref == ref)
            return upval;
            
    // create new upval
    struct lusp_vm_upval_t* result = (struct lusp_vm_upval_t*)lusp_memory_allocate(sizeof(struct lusp_vm_upval_t));
    DL_ASSERT(result);
    
    result->ref = ref;
    result->next = *list;
    *list = result;
    
    return result;
}

static inline void close_upvals(struct lusp_vm_upval_t* list)
{
    while (list)
    {
        struct lusp_vm_upval_t* next = list->next;
        
        list->object = *list->ref;
        list->ref = &list->object;
        
        list = next;
    }
}

static struct lusp_object_t* eval(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t** eval_stack, unsigned int arg_count)
{
    struct lusp_vm_op_t* pc = code->ops;
    
    struct lusp_object_t* value = 0;
    
    struct lusp_object_t** eval_stack_top = eval_stack + code->local_count;
    
    struct lusp_vm_upval_t* upvals = 0;
    
    for (;;)
    {
		struct lusp_vm_op_t op = *pc++;
		
        switch (op.opcode)
        {
        case LUSP_VMOP_GET_OBJECT:
            value = op.get_object.object;
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
            DL_ASSERT(value && (value->type == LUSP_OBJECT_CLOSURE || value->type == LUSP_OBJECT_PROCEDURE));
            
            if (value->type == LUSP_OBJECT_CLOSURE)
            {
                unsigned int count = op.call.count;
                
                eval_stack_top -= count;
				value = value->closure.code->evaluator(value->closure.code, value->closure.closure, eval_stack_top, count);
            }
            else
            {
                unsigned int count = op.call.count;
                
                eval_stack_top -= count;
                value = (value->procedure.code)(code->env, eval_stack_top, count);
            }
            break;
            
        case LUSP_VMOP_RETURN:
            close_upvals(upvals);
            return value;
            
        case LUSP_VMOP_JUMP:
            pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_JUMP_IF:
            if (value != &g_lusp_false) pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_JUMP_IFNOT:
            if (value == &g_lusp_false) pc = pc + op.jump.offset - 1;
            break;
            
        case LUSP_VMOP_CREATE_CLOSURE:
        {
            unsigned int upval_count = op.create_closure.code->upval_count;
            
            value = lusp_mkclosure(op.create_closure.code, upval_count);
            
            struct lusp_vm_closure_t* newclosure = value->closure.closure;
            
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
        }
    }
}

struct lusp_object_t* lusp_eval(struct lusp_object_t* object)
{
	if (!object || object->type != LUSP_OBJECT_CLOSURE) return 0;
	
	struct lusp_object_t* eval_stack[1024];
	
	struct lusp_vm_bytecode_t* code = object->closure.code;
	
    return code->evaluator(code, 0, eval_stack, 0);
}

void lusp_bytecode_setup(struct lusp_vm_bytecode_t* code)
{
	code->evaluator = eval;
}
