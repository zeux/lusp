// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/eval.h>

#include <lusp/vm/bytecode.h>

static lusp_vm_evaluator_t g_evaluator;

// available evaluator functions
struct lusp_object_t lusp_eval_vm(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count);

#if DL_WINDOWS
struct lusp_object_t lusp_eval_jit_x86(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count);
#endif

void lusp_jit_set(bool enabled)
{
#if DL_WINDOWS
    g_evaluator = enabled ? lusp_eval_jit_x86 : lusp_eval_vm;
#else
    (void)enabled;
    g_evaluator = lusp_eval_vm;
#endif
}

bool lusp_jit_get()
{
#if DL_WINDOWS
    return g_evaluator == lusp_eval_jit_x86;
#else
    return false;
#endif
}

struct lusp_object_t lusp_eval(struct lusp_object_t object)
{
    if (object.type != LUSP_OBJECT_CLOSURE) return lusp_mknull();

    struct lusp_object_t eval_stack[1024];

    return g_evaluator(object.closure->code, 0, eval_stack, 0);
}
