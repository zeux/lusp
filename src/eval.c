#include "eval.h"

#include "bytecode.h"

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

	// setup top-level frame
	eval_stack[0].type = LUSP_OBJECT_CALL_FRAME;

	struct lusp_vm_call_frame_t* frame = (struct lusp_vm_call_frame_t*)eval_stack[0].call_frame;

	frame->regs = 0;
	frame->closure = 0;
	frame->pc = 0;

	// call
	return g_evaluator(object.closure->code, object.closure, eval_stack + 2, 0);
}
