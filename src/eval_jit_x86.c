#ifdef DL_WINDOWS

#include "environment.h"
#include "object.h"

#include "bytecode.h"
#include "codegen_x86.h"
#include "utils.h"

#include <windows.h>

void* allocate_code()
{
	return VirtualAlloc(0, 16 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

static struct lusp_vm_upval_t g_dummy_upval = {0, {{LUSP_OBJECT_NULL, {0}}}};

// ecx = ref, edx = unused
static struct lusp_vm_upval_t* __fastcall jit_mkupval(struct lusp_object_t* ref, unsigned int unused, struct lusp_vm_upval_t** list)
{
	(void)unused;

	return mkupval(list, ref);
}

static struct lusp_vm_upval_t* __fastcall jit_close_upvals(struct lusp_vm_upval_t* list, struct lusp_object_t* begin)
{
	return close_upvals(list, begin);
}

#define BINOP(func)                                                                                                  \
	static struct lusp_object_t __fastcall jit_binop_##func(struct lusp_object_t* left, struct lusp_object_t* right) \
	{                                                                                                                \
		return binop_##func(left, right);                                                                            \
	}

typedef struct lusp_object_t(__fastcall* binop_function_t)(struct lusp_object_t*, struct lusp_object_t*);

BINOP(add);
BINOP(subtract);
BINOP(multiply);
BINOP(divide);
BINOP(modulo);
BINOP(equal);
BINOP(not_equal);
BINOP(less);
BINOP(less_equal);
BINOP(greater);
BINOP(greater_equal);

#undef BINOP

// registers:
// ebx: closure
// esi: regs
// ecx: arg_count, used for internal calculations
// eax, edx, edi: used for internal calculations

static inline uint8_t* compile_prologue(uint8_t* code)
{
	// store volatile registers to stack
	PUSH_REG(EBX);
	PUSH_REG(ESI);
	PUSH_REG(EDI);

	// store upval list on stack
	PUSH_IMM(&g_dummy_upval);

	// assuming the following declaration, load arguments from stack:
	// typedef struct lusp_object_t (*lusp_vm_evaluator_t)(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count);
	const unsigned int stack_offset = 20;

	// load arg_count into ecx
	MOV_REG_PREG_OFF(ECX, ESP, stack_offset + 12);

	// load regs into esi
	MOV_REG_PREG_OFF(ESI, ESP, stack_offset + 8);

	// load closure into ebx
	MOV_REG_PREG_OFF(EBX, ESP, stack_offset + 4);

	return code;
}

static inline uint8_t* compile_epilogue(uint8_t* code)
{
	// load upval list from stack
	POP_REG(ECX);

	// load volatile registers from stack
	POP_REG(EDI);
	POP_REG(ESI);
	POP_REG(EBX);

	return code;
}

static inline uint8_t* compile_load_reg(uint8_t* code, unsigned int reg)
{
	MOV_REG_PREG_OFF(EAX, ESI, reg * sizeof(struct lusp_object_t) + 0);
	MOV_REG_PREG_OFF(EDX, ESI, reg * sizeof(struct lusp_object_t) + 4);

	return code;
}

static inline uint8_t* compile_store_reg(uint8_t* code, unsigned int reg)
{
	MOV_PREG_OFF_REG(ESI, reg * sizeof(struct lusp_object_t) + 0, EAX);
	MOV_PREG_OFF_REG(ESI, reg * sizeof(struct lusp_object_t) + 4, EDX);

	return code;
}

static inline uint8_t* compile_load_const(uint8_t* code, struct lusp_vm_op_t op)
{
	// load object from fixed address
	MOV_REG_PIMM(EAX, &op.load_const.object->type);
	MOV_REG_PIMM(EDX, &op.load_const.object->object);

	// store to regs
	return compile_store_reg(code, op.reg);
}

static inline uint8_t* compile_loadstore_global(uint8_t* code, struct lusp_vm_op_t op)
{
	// object is in environment slot
	struct lusp_object_t* value = &op.loadstore_global.slot->value;

	if (op.opcode == LUSP_VMOP_LOAD_GLOBAL)
	{
		// load from slot
		MOV_REG_PIMM(EAX, &value->type);
		MOV_REG_PIMM(EDX, &value->object);

		// store to regs
		code = compile_store_reg(code, op.reg);
	}
	else
	{
		// load from regs
		code = compile_load_reg(code, op.reg);

		// store to slot
		MOV_PIMM_REG(&value->type, EAX);
		MOV_PIMM_REG(&value->object, EDX);
	}

	return code;
}

static inline uint8_t* compile_loadstore_upval(uint8_t* code, struct lusp_vm_op_t op)
{
	// object is in upvalue
	size_t offset = offsetof(struct lusp_vm_closure_t, upvals[op.loadstore_upval.index]);

	// load upval address
	MOV_REG_PREG_OFF(ECX, EBX, offset);
	MOV_REG_PREG_OFF(ECX, ECX, offsetof(struct lusp_vm_upval_t, ref));

	if (op.opcode == LUSP_VMOP_LOAD_UPVAL)
	{
		// load from upval
		MOV_REG_PREG_OFF(EAX, ECX, 0);
		MOV_REG_PREG_OFF(EDX, ECX, 4);

		// store to regs
		code = compile_store_reg(code, op.reg);
	}
	else
	{
		// load from regs
		code = compile_load_reg(code, op.reg);

		// store to upval
		MOV_PREG_OFF_REG(ECX, 0, EAX);
		MOV_PREG_OFF_REG(ECX, 4, EDX);
	}

	return code;
}

static inline uint8_t* compile_move(uint8_t* code, struct lusp_vm_op_t op)
{
	// load from regs
	code = compile_load_reg(code, op.move.index);

	// store to regs
	return compile_store_reg(code, op.reg);
}

static inline uint8_t* compile_call(uint8_t* code, struct lusp_vm_op_t op, struct lusp_environment_t* env)
{
	// load from regs
	code = compile_load_reg(code, op.reg);

	// compute args start
	LEA_REG_PREG_OFF(ECX, ESI, op.call.args * sizeof(struct lusp_object_t));

	// is this a function?
	DL_STATIC_ASSERT(offsetof(struct lusp_object_t, type) == 0);
	CMP_REG_IMM8(EAX, LUSP_OBJECT_FUNCTION);

	// it's not, jump to closure call
	uint8_t* closure;
	JNE_IMM8(closure);

	// push arguments (environment pointer, argument array, call count)
	PUSH_IMM(op.call.count);
	PUSH_REG(ECX);
	PUSH_IMM(env);

	// call function by pointer
	CALL_REG(EDX);

	// pop arguments
	ADD_REG_IMM8(ESP, 12);

	// jmp end
	uint8_t* end;
	JMP_IMM8(end);

	// closure:
	LABEL8(closure);

	// load bytecode pointer
	MOV_REG_PREG_OFF(EAX, EDX, offsetof(struct lusp_vm_closure_t, code));

	// push arguments (bytecode, closure, argument array, call count)
	PUSH_IMM(op.call.count);
	PUSH_REG(ECX);
	PUSH_REG(EDX);
	PUSH_REG(EAX);

	// call closure
	MOV_REG_PREG_OFF(EAX, EAX, offsetof(struct lusp_vm_bytecode_t, jit));
	CALL_REG(EAX);

	// pop arguments
	ADD_REG_IMM8(ESP, 16);

	// end:
	LABEL8(end);

	// store to regs
	return compile_store_reg(code, op.reg);
}

static inline uint8_t* compile_ret(uint8_t* code, struct lusp_vm_op_t op)
{
	// load from regs
	code = compile_load_reg(code, op.reg);

	// epilogue
	code = compile_epilogue(code);

	// ret
	RET();

	return code;
}

static inline uint8_t* compile_jump(uint8_t* code, struct lusp_vm_op_t op)
{
	(void)op;

	// jmp offset
	JMP_IMM32(0);

	return code;
}

static inline uint8_t* compile_jump_cond(uint8_t* code, struct lusp_vm_op_t op)
{
	// load from regs
	code = compile_load_reg(code, op.reg);

	// merge type and boolean value in single value
	MOV_REG_REG(ECX, EDX);
	SHL_REG_IMM8(ECX, 24);
	ADD_REG_REG(ECX, EAX);

	// compare with 'false' value and jump
	CMP_REG_IMM8(ECX, LUSP_OBJECT_BOOLEAN);
	(op.opcode == LUSP_VMOP_JUMP_IFNOT) ? (JE_IMM32(0)) : (JNE_IMM32(0));

	return code;
}

static inline uint8_t* compile_create_closure(uint8_t* code, struct lusp_vm_op_t op, struct lusp_vm_op_t* upval_ops)
{
	unsigned int upval_count = op.create_closure.code->upval_count;

	// push arguments (bytecode, upvalue count)
	PUSH_IMM(upval_count);
	PUSH_IMM(op.create_closure.code);

	// create closure
	CALL_FUNC(lusp_mkclosure);

	// pop arguments
	ADD_REG_IMM8(ESP, 8);

	// if closure has no upvalues, we're done
	if (upval_count == 0) return compile_store_reg(code, op.reg);

	// save closure pointer
	MOV_REG_REG(EDI, EDX);

	// set upvalues
	for (unsigned int i = 0; i < upval_count; ++i)
	{
		struct lusp_vm_op_t op = upval_ops[i];

		switch (op.opcode)
		{
		case LUSP_VMOP_MOVE:
			// push arguments (ref, list)
			LEA_REG_PREG_OFF(ECX, ESI, op.move.index * sizeof(struct lusp_object_t));
			PUSH_REG(ESP);

			// make upval
			CALL_FUNC(jit_mkupval);
			break;

		case LUSP_VMOP_LOAD_UPVAL:
			// get upval from closure upval list
			MOV_REG_PREG_OFF(EAX, EBX, offsetof(struct lusp_vm_closure_t, upvals[op.loadstore_upval.index]));
			break;

		default:
			assert(!"unexpected instruction");
		}

		// store upval
		MOV_PREG_OFF_REG(EDI, offsetof(struct lusp_vm_closure_t, upvals[i]), EAX);
	}

	// fix closure
	MOV_REG_IMM(EAX, LUSP_OBJECT_CLOSURE);
	MOV_REG_REG(EDX, EDI);

	// store to reg
	return compile_store_reg(code, op.reg);
}

static inline uint8_t* compile_close(uint8_t* code, struct lusp_vm_op_t op)
{
	// push arguments (upval list, begin)
	POP_REG(ECX);
	LEA_REG_PREG_OFF(EDX, ESI, op.close.begin * sizeof(struct lusp_object_t));

	// close
	CALL_FUNC(jit_close_upvals);

	// store upval list
	PUSH_REG(EAX);

	return code;
}

static inline uint8_t* compile_binop(uint8_t* code, struct lusp_vm_op_t op, binop_function_t function)
{
	// push arguments (left, right)
	LEA_REG_PREG_OFF(ECX, ESI, op.binop.left * sizeof(struct lusp_object_t));
	LEA_REG_PREG_OFF(EDX, ESI, op.binop.right * sizeof(struct lusp_object_t));

	// call function
	CALL_FUNC(function);

	// store to reg
	return compile_store_reg(code, op.reg);
}

static void compile(uint8_t* code, struct lusp_environment_t* env, struct lusp_vm_op_t* ops, unsigned int op_count)
{
	uint8_t* labels[1024];
	uint8_t* jumps[1024];

	// prologue
	code = compile_prologue(code);

	// first pass: compile code
	for (unsigned int i = 0; i < op_count; ++i)
	{
		struct lusp_vm_op_t op = ops[i];

		// store label
		labels[i] = code;

		// compile code
		switch (op.opcode)
		{
		case LUSP_VMOP_LOAD_CONST:
			code = compile_load_const(code, op);
			break;

		case LUSP_VMOP_LOAD_GLOBAL:
		case LUSP_VMOP_STORE_GLOBAL:
			code = compile_loadstore_global(code, op);
			break;

		case LUSP_VMOP_LOAD_UPVAL:
		case LUSP_VMOP_STORE_UPVAL:
			code = compile_loadstore_upval(code, op);
			break;

		case LUSP_VMOP_MOVE:
			code = compile_move(code, op);
			break;

		case LUSP_VMOP_CALL:
			code = compile_call(code, op, env);
			break;

		case LUSP_VMOP_RETURN:
			code = compile_ret(code, op);
			break;

		case LUSP_VMOP_JUMP:
			code = compile_jump(code, op);
			jumps[i] = code - sizeof(uint32_t);
			break;

		case LUSP_VMOP_JUMP_IF:
		case LUSP_VMOP_JUMP_IFNOT:
			code = compile_jump_cond(code, op);
			jumps[i] = code - sizeof(uint32_t);
			break;

		case LUSP_VMOP_CREATE_CLOSURE:
			code = compile_create_closure(code, op, &ops[i + 1]);

			// skip upvalue instructions
			i += op.create_closure.code->upval_count;
			break;

		case LUSP_VMOP_CLOSE:
			code = compile_close(code, op);
			break;

		case LUSP_VMOP_ADD:
			code = compile_binop(code, op, jit_binop_add);
			break;

		case LUSP_VMOP_SUBTRACT:
			code = compile_binop(code, op, jit_binop_subtract);
			break;

		case LUSP_VMOP_MULTIPLY:
			code = compile_binop(code, op, jit_binop_multiply);
			break;

		case LUSP_VMOP_DIVIDE:
			code = compile_binop(code, op, jit_binop_divide);
			break;

		case LUSP_VMOP_MODULO:
			code = compile_binop(code, op, jit_binop_modulo);
			break;

		case LUSP_VMOP_EQUAL:
			code = compile_binop(code, op, jit_binop_equal);
			break;

		case LUSP_VMOP_NOT_EQUAL:
			code = compile_binop(code, op, jit_binop_not_equal);
			break;

		case LUSP_VMOP_LESS:
			code = compile_binop(code, op, jit_binop_less);
			break;

		case LUSP_VMOP_LESS_EQUAL:
			code = compile_binop(code, op, jit_binop_less_equal);
			break;

		case LUSP_VMOP_GREATER:
			code = compile_binop(code, op, jit_binop_greater);
			break;

		case LUSP_VMOP_GREATER_EQUAL:
			code = compile_binop(code, op, jit_binop_greater_equal);
			break;

		default:
			assert(false);
		}
	}

	// second pass: fixup labels
	for (unsigned int i = 0; i < op_count; ++i)
	{
		struct lusp_vm_op_t op = ops[i];

		if (op.opcode != LUSP_VMOP_JUMP && op.opcode != LUSP_VMOP_JUMP_IF && op.opcode != LUSP_VMOP_JUMP_IFNOT) continue;

		LABEL32(jumps[i], labels[i + op.jump.offset + 1]);
	}
}

static void compile_jit(struct lusp_vm_bytecode_t* code)
{
	code->jit = allocate_code();

	compile((unsigned char*)code->jit, code->env, code->ops, code->op_count);
}

struct lusp_object_t lusp_eval_jit_x86_stub(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count)
{
	if (code->jit == lusp_eval_jit_x86_stub) compile_jit(code);

	lusp_vm_evaluator_t function = (lusp_vm_evaluator_t)code->jit;

	return function(code, closure, regs, arg_count);
}

struct lusp_object_t lusp_eval_jit_x86(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count)
{
	lusp_vm_evaluator_t function = (lusp_vm_evaluator_t)code->jit;

	return function(code, closure, regs, arg_count);
}

#endif
