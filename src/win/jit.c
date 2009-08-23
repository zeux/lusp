// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/jit.h>

#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/object.h>
#include <lusp/evalutils.h>
#include <lusp/win/assembler.h>

#include <windows.h>

void* allocate_code()
{
	return VirtualAlloc(0, 16*1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

struct lusp_vm_upval_t dummy_upval = {};

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

static struct lusp_object_t __fastcall jit_create_list(struct lusp_object_t* end, struct lusp_object_t* begin)
{
	return create_list(begin, end);
}

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
	PUSH_IMM32(&dummy_upval);
	
	// assuming the following declaration, load arguments from stack:
	// typedef struct lusp_object_t (*lusp_vm_evaluator_t)(struct lusp_vm_bytecode_t* code, struct lusp_vm_closure_t* closure, struct lusp_object_t* regs, unsigned int arg_count);
	const unsigned int stack_offset = 20;
	
	// load arg_count into ecx
	MOV_REG_PREG_OFF8(ECX, ESP, stack_offset + 12);
	
	// load regs into esi
	MOV_REG_PREG_OFF8(ESI, ESP, stack_offset + 8);
	
	// load closure into ebx
	MOV_REG_PREG_OFF8(EBX, ESP, stack_offset + 4);
	
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
	MOV_REG_PREG_OFF32(EAX, ESI, reg * sizeof(struct lusp_object_t) + 0);
	MOV_REG_PREG_OFF32(EDX, ESI, reg * sizeof(struct lusp_object_t) + 4);
	
	return code;
}

static inline uint8_t* compile_store_reg(uint8_t* code, unsigned int reg)
{
	MOV_PREG_OFF32_REG(ESI, reg * sizeof(struct lusp_object_t) + 0, EAX);
	MOV_PREG_OFF32_REG(ESI, reg * sizeof(struct lusp_object_t) + 4, EDX);
	
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
	DL_STATIC_ASSERT(offsetof(struct lusp_vm_upval_t, ref) == 0);
	
	MOV_REG_PREG_OFF32(ECX, EBX, offset);
	MOV_REG_PREG(ECX, ECX);
			
	if (op.opcode == LUSP_VMOP_LOAD_UPVAL)
	{
		// load from upval
		MOV_REG_PREG(EAX, ECX);
		MOV_REG_PREG_OFF8(EDX, ECX, 4);
		
		// store to regs
		code = compile_store_reg(code, op.reg);
	}
	else
	{
		// load from regs
		code = compile_load_reg(code, op.reg);
		
		// store to upval
		MOV_PREG_REG(ECX, EAX);
		MOV_PREG_OFF8_REG(ECX, 4, EDX);
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
	LEA_REG_PREG_OFF32(ECX, ESI, op.call.args * sizeof(struct lusp_object_t));
	
	// is this a function?
	DL_STATIC_ASSERT(offsetof(struct lusp_object_t, type) == 0);
	CMP_REG_IMM8(EAX, LUSP_OBJECT_FUNCTION);

	// it's not, jump to closure call
	uint8_t* closure;
	JNE_IMM8(closure);

	// push arguments (environment pointer, argument array, call count)
	PUSH_IMM32(op.call.count);
	PUSH_REG(ECX);
	PUSH_IMM32(env);

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
	MOV_REG_PREG_OFF8(EAX, EDX, offsetof(struct lusp_vm_closure_t, code));
	
	// push arguments (bytecode, closure, argument array, call count)
	PUSH_IMM32(op.call.count);
	PUSH_REG(ECX);
	PUSH_REG(EDX);
	PUSH_REG(EAX);
	
	// call evaluator
	MOV_REG_PREG_OFF8(EAX, EAX, offsetof(struct lusp_vm_bytecode_t, evaluator));
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
	// $$$: remove this
	lusp_compile_jit(op.create_closure.code);
	
	unsigned int upval_count = op.create_closure.code->upval_count;

	// push arguments (bytecode, upvalue count)
	PUSH_IMM32(upval_count);
	PUSH_IMM32(op.create_closure.code);
	
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
			LEA_REG_PREG_OFF32(ECX, ESI, op.move.index * sizeof(struct lusp_object_t));
			PUSH_REG(ESP);
			
			// make upval
			CALL_FUNC(jit_mkupval);
			break;

		case LUSP_VMOP_LOAD_UPVAL:
			// get upval from closure upval list
			MOV_REG_PREG_OFF32(EAX, EBX, offsetof(struct lusp_vm_closure_t, upvals[op.loadstore_upval.index]));
			break;

		default:
			DL_ASSERT(!"unexpected instruction");
		}
		
		// store upval
		MOV_PREG_OFF32_REG(EDI, offsetof(struct lusp_vm_closure_t, upvals[i]), EAX);
	}
	
	// fix closure
	MOV_REG_IMM32(EAX, LUSP_OBJECT_CLOSURE);
	MOV_REG_REG(EDX, EDI);
	
	// store to reg
	return compile_store_reg(code, op.reg);
}

static inline uint8_t* compile_create_list(uint8_t* code, struct lusp_vm_op_t op)
{
	size_t offset = op.reg * sizeof(struct lusp_object_t);
	
	// compute end of range
	SHL_REG_IMM8(ECX, 3);
	ADD_REG_REG(ECX, ESI);
	
	// compute start of range
	LEA_REG_PREG_OFF32(EDX, ESI, offset);
	
	// create list
	CALL_FUNC(jit_create_list);
	
	// store to reg
	return compile_store_reg(code, op.reg);
}

static inline uint8_t* compile_close(uint8_t* code, struct lusp_vm_op_t op)
{
	// push arguments (upval list, begin)
	POP_REG(ECX);
	LEA_REG_PREG_OFF32(EDX, ESI, op.close.begin * sizeof(struct lusp_object_t));
	
	// close
	CALL_FUNC(jit_close_upvals);
	
	// store upval list
	PUSH_REG(EAX);
	
	return code;
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
			
		case LUSP_VMOP_CREATE_LIST:
			code = compile_create_list(code, op);
			break;
			
		case LUSP_VMOP_CLOSE:
			code = compile_close(code, op);
			break;

		default:
			DL_ASSERT(false);
		}
	}
	
	// second pass: fixup labels
	for (unsigned int i = 0; i < op_count; ++i)
	{
		struct lusp_vm_op_t op = ops[i];
		
		if (op.opcode != LUSP_VMOP_JUMP && op.opcode != LUSP_VMOP_JUMP_IF && op.opcode != LUSP_VMOP_JUMP_IFNOT) continue;
		
		LABEL32(jumps[i], labels[i + op.jump.offset]);
	}
}

void lusp_compile_jit(struct lusp_vm_bytecode_t* code)
{
	if (code->jit) return;
	
	code->jit = allocate_code();
	
	compile((unsigned char*)code->jit, code->env, code->ops, code->op_count);
	
	code->evaluator = (lusp_vm_evaluator_t)code->jit;
}