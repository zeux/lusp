// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/object.h>
#include <lusp/jit.h>

#include <core/memory.h>
#include <mem/arena.h>

extern struct mem_arena_t g_lusp_heap;

extern struct lusp_object_t g_lusp_false;

static inline struct lusp_vm_bind_frame_t* create_frame_impl(struct lusp_vm_bind_frame_t* parent, struct lusp_object_t** values, unsigned int count, unsigned int copy_count)
{
    DL_ASSERT(count >= copy_count);
    
    struct lusp_vm_bind_frame_t* result = (struct lusp_vm_bind_frame_t*)mem_arena_allocate(&g_lusp_heap,
		offsetof(struct lusp_vm_bind_frame_t, binds) + count * sizeof(struct lusp_object_t*),
		alignof(struct lusp_vm_bind_frame_t));
    DL_ASSERT(result);
    
    result->parent = parent;
    result->count = count;
    
    memcpy(result->binds, values, copy_count * sizeof(struct lusp_object_t*));
    
    return result;
}

static inline struct lusp_vm_bind_frame_t* create_frame(struct lusp_vm_bind_frame_t* parent, struct lusp_object_t** values, unsigned int count)
{
    return create_frame_impl(parent, values, count, count);
}

static inline struct lusp_vm_bind_frame_t* create_frame_rest(struct lusp_vm_bind_frame_t* parent, struct lusp_object_t** values, unsigned int count, unsigned int rest_count)
{
    struct lusp_vm_bind_frame_t* result = create_frame_impl(parent, values, count + 1, count);
    DL_ASSERT(result);
    
    // create rest list
    struct lusp_object_t* rest = 0;
    
    for (unsigned int i = rest_count; i > 0; --i) rest = lusp_mkcons(values[i - 1], rest);
    
    // bind rest argument
    result->binds[count] = rest;
    
    return result;
    
}

static inline struct lusp_vm_bind_frame_t* get_frame(struct lusp_vm_bind_frame_t* top, unsigned int depth)
{
    while (depth)
    {
        DL_ASSERT(top);
        top = top->parent;
        depth--;
    }
    
    return top;
}

static struct lusp_object_t* eval(struct lusp_vm_bytecode_t* code, struct lusp_vm_bind_frame_t* bind_frame, struct lusp_object_t** eval_stack, unsigned int arg_count)
{
    struct lusp_object_t* value = 0;
    unsigned int pc = 0;
    
    for (;;)
    {
        struct lusp_vm_op_t* op = &code->ops[pc++];
        
        switch (op->opcode)
        {
        case LUSP_VMOP_GET_OBJECT:
            value = op->get_object.object;
            break;
            
        case LUSP_VMOP_GET_LOCAL:
        {
            struct lusp_vm_bind_frame_t* frame = get_frame(bind_frame, op->getset_local.depth);
            DL_ASSERT(op->getset_local.index < frame->count);
            value = frame->binds[op->getset_local.index];
        } break;
            
        case LUSP_VMOP_SET_LOCAL:
        {
            struct lusp_vm_bind_frame_t* frame = get_frame(bind_frame, op->getset_local.depth);
            DL_ASSERT(op->getset_local.index < frame->count);
            frame->binds[op->getset_local.index] = value;
        } break;
            
        case LUSP_VMOP_GET_GLOBAL:
            value = op->getset_global.slot->value;
            break;
            
        case LUSP_VMOP_SET_GLOBAL:
            op->getset_global.slot->value = value;
            break;
            
        case LUSP_VMOP_PUSH:
            *eval_stack++ = value;
            break;
            
        case LUSP_VMOP_BIND:
            eval_stack -= op->bind.count;
            bind_frame = create_frame(bind_frame, eval_stack, op->bind.count);
            break;
            
        case LUSP_VMOP_BIND_REST:
            DL_ASSERT(arg_count >= op->bind.count);
            eval_stack -= arg_count;
            bind_frame = create_frame_rest(bind_frame, eval_stack, op->bind.count, arg_count - op->bind.count);
            break;
            
        case LUSP_VMOP_UNBIND:
            DL_ASSERT(bind_frame);
            bind_frame = bind_frame->parent;
            break;
            
        case LUSP_VMOP_CALL:
            DL_ASSERT(value && (value->type == LUSP_OBJECT_CLOSURE || value->type == LUSP_OBJECT_PROCEDURE));
            
            if (value->type == LUSP_OBJECT_CLOSURE)
            {
                unsigned int count = op->call.count;
                
				value = value->closure.code->evaluator(value->closure.code, value->closure.frame, eval_stack, count);
                eval_stack -= count;
            }
            else
            {
                unsigned int count = op->call.count;
                
                eval_stack -= count;
                value = (value->procedure.code)(code->env, eval_stack, count);
            }
            break;
            
        case LUSP_VMOP_RETURN:
            return value;
            
        case LUSP_VMOP_JUMP:
            pc = op->jump.index;
            break;
            
        case LUSP_VMOP_JUMP_IF:
            if (value != &g_lusp_false) pc = op->jump.index;
            break;
            
        case LUSP_VMOP_JUMP_IFNOT:
            if (value == &g_lusp_false) pc = op->jump.index;
            break;
            
        case LUSP_VMOP_CREATE_CLOSURE:
            value = lusp_mkclosure(bind_frame, op->create_closure.code);
            break;
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
