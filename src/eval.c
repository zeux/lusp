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

struct continuation_t
{
    struct lusp_vm_bind_frame_t* bind_frame;
    
    // caller code
    struct lusp_vm_bytecode_t* code;
    unsigned int pc;
};

static inline struct lusp_vm_bind_frame_t* create_frame_impl(struct lusp_vm_bind_frame_t* parent, struct lusp_object_t** values, unsigned int count, unsigned int copy_count)
{
    DL_ASSERT(count >= copy_count);
    
    struct lusp_vm_bind_frame_t* result = MEM_ARENA_NEW(&g_lusp_heap, struct lusp_vm_bind_frame_t);
    DL_ASSERT(result);
    
    result->parent = parent;
    result->count = count;
    result->binds = MEM_ARENA_NEW_ARRAY(&g_lusp_heap, struct lusp_object_t*, count);
    DL_ASSERT(result->binds);
    
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

static struct lusp_object_t* eval(struct lusp_vm_bytecode_t* bytecode)
{
    struct continuation_t continuation_stack[1024];
    unsigned int continuation_stack_top = 0;
    
    struct lusp_object_t* eval_stack[1024];
    unsigned int eval_stack_top = 0;
    
    struct lusp_vm_bind_frame_t* bind_frame = 0;
    
    struct lusp_object_t* value = 0;
    
    struct lusp_vm_bytecode_t* code = bytecode;
    unsigned int pc = 0;
    
    unsigned int call_count = 0;
    
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
            DL_ASSERT(eval_stack_top < sizeof(eval_stack) / sizeof(eval_stack[0]));
            eval_stack[eval_stack_top++] = value;
            break;
            
        case LUSP_VMOP_BIND:
            DL_ASSERT(eval_stack_top >= op->bind.count);
            bind_frame = create_frame(bind_frame, eval_stack + eval_stack_top - op->bind.count, op->bind.count);
            eval_stack_top -= op->bind.count;
            break;
            
        case LUSP_VMOP_BIND_REST:
            DL_ASSERT(eval_stack_top >= op->bind.count && call_count >= op->bind.count);
            bind_frame = create_frame_rest(bind_frame, eval_stack + eval_stack_top - call_count, op->bind.count, call_count - op->bind.count);
            eval_stack_top -= call_count;
            break;
            
        case LUSP_VMOP_UNBIND:
            DL_ASSERT(bind_frame);
            bind_frame = bind_frame->parent;
            break;
            
        case LUSP_VMOP_CALL:
            DL_ASSERT(eval_stack_top >= op->call.count);
            DL_ASSERT(value && (value->type == LUSP_OBJECT_CLOSURE || value->type == LUSP_OBJECT_PROCEDURE));
            
            if (value->type == LUSP_OBJECT_CLOSURE)
            {
                // push continuation
                DL_ASSERT(continuation_stack_top < sizeof(continuation_stack) / sizeof(continuation_stack[0]));
                continuation_stack[continuation_stack_top].bind_frame = bind_frame;
                continuation_stack[continuation_stack_top].code = code;
                continuation_stack[continuation_stack_top].pc = pc;
                continuation_stack_top++;
                
                // call
                bind_frame = value->closure.frame;
                code = value->closure.code;
                pc = 0;
                call_count = op->call.count;
            }
            else
            {
                unsigned int count = op->call.count;
                
                value = (value->procedure.code)(code->env, eval_stack + eval_stack_top - count, count);
                eval_stack_top -= count;
            }
            break;
            
        case LUSP_VMOP_RETURN:
            if (continuation_stack_top == 0)
            {
                // top-level return
                DL_ASSERT(eval_stack_top == 0 && bind_frame == 0);
                return value;
            }
            
            continuation_stack_top--;
            bind_frame = continuation_stack[continuation_stack_top].bind_frame;
            code = continuation_stack[continuation_stack_top].code;
            pc = continuation_stack[continuation_stack_top].pc;
            break;
            
        case LUSP_VMOP_JUMP:
            pc = op->jump.index;
            break;
            
        case LUSP_VMOP_JUMP_IFNOT:
            if (value && value->type == LUSP_OBJECT_BOOLEAN && value->boolean.value == false)
                pc = op->jump.index;
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
	
	struct lusp_vm_bytecode_t* code = object->closure.code;
	
	if (!code->jit) lusp_compile_jit(code);
	
	return lusp_eval_jit(code);
	
    return eval(code);
}