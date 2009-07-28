// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/jit.h>

#include <lusp/vm.h>
#include <lusp/environment.h>
#include <lusp/object.h>
#include <lusp/win/assembler.h>

#include <mem/arena.h>

#include <windows.h>

extern struct mem_arena_t g_lusp_heap;

extern struct lusp_object_t g_lusp_false;

void* allocate_code()
{
	void* result = mem_arena_allocate(&g_lusp_heap, 16*1024, 4096);
	DL_ASSERT(result);
	
	DWORD flags;
	VirtualProtect(result, 16*1024, PAGE_EXECUTE_READWRITE, &flags);
	
	return result;
}

// registers:
// eax: value
// ebx: bind_frame
// ecx: used for internal calculations
// edx: eval_stack
// esi: used for internal calculations

static inline uint8_t* compile_prologue(uint8_t* code)
{
	// push ebx
	PUSH_REG(EBX);
	
	// push esi
	PUSH_REG(ESI);
	
	// since we pushed two values on stack, evaluator arguments are available at [esp + 12], [esp + 16], etc.
	
	// mov ebx, dword ptr [esp + 16]
	MOV_REG_PESP_OFF8(EBX, 16);
	
	// mov edx, dword ptr [esp + 20]
	MOV_REG_PESP_OFF8(EDX, 20);
	
	return code;
}

static inline uint8_t* compile_epilogue(uint8_t* code)
{
	// pop esi
	POP_REG(ESI);
	
	// pop ebx
	POP_REG(EBX);
	
	return code;
}

static inline uint8_t* compile_get_object(uint8_t* code, struct lusp_vm_op_t op)
{
	// mov eax, imm32
	MOV_REG_IMM32(EAX, op.get_object.object);
	
	return code;
}

static inline uint8_t* compile_getset_local(uint8_t* code, struct lusp_vm_op_t op)
{
	unsigned int offset = op.getset_local.index * sizeof(struct lusp_object_t*) +
		offsetof(struct lusp_vm_bind_frame_t, binds);
			
	if (op.getset_local.depth == 0)
	{
		// mov eax, dword ptr [ebx + offset]
		// mov dword ptr [ebx + offset], eax
		(op.opcode == LUSP_VMOP_GET_LOCAL) ? (MOV_REG_PREG_OFF32(EAX, EBX, offset)) : (MOV_PREG_OFF32_REG(EBX, offset, EAX));
	}
	else
	{
		DL_STATIC_ASSERT(offsetof(struct lusp_vm_bind_frame_t, parent) == 0);
		
		// mov ecx, dword ptr [ebx]
		MOV_REG_PREG(ECX, EBX);

		// mov ecx, dword ptr [ecx]
		for (unsigned int i = 1; i < op.getset_local.depth; ++i) MOV_REG_PREG(ECX, ECX);

		// mov eax, dword ptr [ecx + offset]
		// mov dword ptr [ecx + offset], eax
		(op.opcode == LUSP_VMOP_GET_LOCAL) ? (MOV_REG_PREG_OFF32(EAX, ECX, offset)) : (MOV_PREG_OFF32_REG(ECX, offset, EAX));
	}
	
	return code;
}

static inline uint8_t* compile_getset_global(uint8_t* code, struct lusp_vm_op_t op)
{
	// mov eax, dword ptr [&slot->value]
	// mov dword ptr [&slot->value], eax
	struct lusp_object_t** value = &op.getset_global.slot->value;
	(op.opcode == LUSP_VMOP_GET_GLOBAL) ? (MOV_EAX_ADDR(value)) : (MOV_ADDR_EAX(value));
	
	return code;
}

static inline uint8_t* compile_push(uint8_t* code, struct lusp_vm_op_t op)
{
	(void)op;
	
	// mov dword ptr [edx], eax
	MOV_PREG_REG(EDX, EAX);

	// add edx, sizeof(struct lusp_object_t*)
	ADD_EDX_IMM8(sizeof(struct lusp_object_t*));
	
	return code;
}

static inline uint8_t* compile_bind(uint8_t* code, struct lusp_vm_op_t op)
{
	unsigned int bind_size = op.bind.count * sizeof(struct lusp_object_t*);
	
	// mov ecx, heap.current
	MOV_ECX_ADDR(&g_lusp_heap.current);

	// add heap.current, bind_size + offsetof(struct lusp_vm_bind_frame_t, binds)
	ADD_ADDR_IMM32(&g_lusp_heap.current, bind_size + offsetof(struct lusp_vm_bind_frame_t, binds));

	// mov dword ptr [ecx], ebx
	DL_STATIC_ASSERT(offsetof(struct lusp_vm_bind_frame_t, parent) == 0);
	MOV_PREG_REG(ECX, EBX);
	
	// mov dword ptr [ecx + offset], op.bind.count
	MOV_PREG_OFF8_IMM32(ECX, offsetof(struct lusp_vm_bind_frame_t, count), op.bind.count);
	
	// sub edx, bind_size
	SUB_EDX_IMM32(bind_size);

	// mov esi, dword ptr [edx + offset]
	// mov dword ptr [ecx + offset], esi
	for (unsigned int j = 0; j < op.bind.count; ++j)
	{
		MOV_REG_PREG_OFF32(ESI, EDX, j * sizeof(struct lusp_object_t*));
		MOV_PREG_OFF32_REG(ECX, j * sizeof(struct lusp_object_t*) + offsetof(struct lusp_vm_bind_frame_t, binds), ESI);
	}

	// mov ebx, ecx
	MOV_REG_REG(EBX, ECX);
	
	return code;
}

static inline uint8_t* compile_unbind(uint8_t* code, struct lusp_vm_op_t op)
{
	(void)op;
	
	// mov ebx, dword ptr [ebx]
	DL_STATIC_ASSERT(offsetof(struct lusp_vm_bind_frame_t, parent) == 0);
	MOV_REG_PREG(EBX, EBX);
	
	return code;
}

static inline uint8_t* compile_call(uint8_t* code, struct lusp_vm_op_t op, struct lusp_environment_t* env)
{
	// cmp dword ptr [eax], LUSP_OBJECT_PROCEDURE
	DL_STATIC_ASSERT(offsetof(struct lusp_object_t, type) == 0);
	CMP_PREG_IMM8(EAX, LUSP_OBJECT_PROCEDURE);

	// jne closure
	uint8_t* closure;
	JNE_IMM8(closure);

	// push count
	PUSH_IMM32(op.call.count);

	// sub edx, count * sizeof(struct lusp_object_t*)
	SUB_EDX_IMM32(op.call.count * sizeof(struct lusp_object_t*));

	// push edx
	PUSH_REG(EDX);

	// push env
	PUSH_IMM32(env);

	// mov eax, dword ptr [eax + offset]
	MOV_REG_PREG_OFF8(EAX, EAX, offsetof(struct lusp_object_t, procedure.code));

	// call eax
	CALL_REG(EAX);

	// pop edx
	POP_REG(EDX);

	// pop edx
	POP_REG(EDX);

	// pop ecx
	POP_REG(ECX);

	// jmp end
	uint8_t* end;
	JMP_IMM8(end);

	// closure:
	LABEL8(closure);
	
	// push count
	PUSH_IMM32(op.call.count);
	
	// push edx
	PUSH_REG(EDX);
	
	// push closure.frame
	PUSH_PREG_OFF8(EAX, offsetof(struct lusp_object_t, closure.frame));
	
	// mov eax, dword ptr [eax + offset]
	MOV_REG_PREG_OFF8(EAX, EAX, offsetof(struct lusp_object_t, closure.code));

	// push eax
	PUSH_REG(EAX);
	
	// mov eax, dword ptr [eax + offset]
	MOV_REG_PREG_OFF8(EAX, EAX, offsetof(struct lusp_vm_bytecode_t, evaluator));

	// call eax
	CALL_REG(EAX);

	// pop edx
	POP_REG(EDX);
	
	// pop edx
	POP_REG(EDX);
	
	// pop edx
	POP_REG(EDX);

	// sub edx, count * sizeof(struct lusp_object_t*)
	SUB_EDX_IMM32(op.call.count * sizeof(struct lusp_object_t*));
	
	// pop ecx
	POP_REG(ECX);
	
	// end:
	LABEL8(end);
	
	return code;
}

static inline uint8_t* compile_ret(uint8_t* code, struct lusp_vm_op_t op)
{
	(void)op;
	
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
	// cmp eax, &g_lusp_false
	CMP_EAX_IMM32(&g_lusp_false);

	// je label
	// jne label
	(op.opcode == LUSP_VMOP_JUMP_IFNOT) ? (JE_IMM32(0)) : (JNE_IMM32(0));

	return code;
}

static inline uint8_t* compile_create_closure(uint8_t* code, struct lusp_vm_op_t op)
{
	// mov eax, heap.current
	MOV_EAX_ADDR(&g_lusp_heap.current);

	// add heap.current, sizeof(struct lusp_object_t)
	ADD_ADDR_IMM32(&g_lusp_heap.current, sizeof(struct lusp_object_t));

	// mov dword ptr [eax], LUSP_OBJECT_CLOSURE
	DL_ASSERT(offsetof(struct lusp_object_t, type) == 0);
	MOV_PEAX_IMM32(LUSP_OBJECT_CLOSURE);

	// mov dword ptr [eax + offset], ebx
	MOV_PREG_OFF8_REG(EAX, offsetof(struct lusp_object_t, closure.frame), EBX);

	// mov dword ptr [eax + offset], op.create_closure.code
	MOV_PREG_OFF8_IMM32(EAX, offsetof(struct lusp_object_t, closure.code), op.create_closure.code);
	
	// $$$: remove this
	lusp_compile_jit(op.create_closure.code);

	return code;
}

static void compile(uint8_t* code, struct lusp_environment_t* env, struct lusp_vm_op_t* ops, unsigned int count)
{
	uint8_t* labels[1024];
	uint8_t* jumps[1024];
	
	// prologue
	code = compile_prologue(code);
	
	// first pass: compile code
	for (unsigned int i = 0; i < count; ++i)
	{
		struct lusp_vm_op_t op = ops[i];
		
		// store label
		labels[i] = code;
		
		// compile code
		switch (op.opcode)
		{
		case LUSP_VMOP_GET_OBJECT:
			code = compile_get_object(code, op);
			break;
			
		case LUSP_VMOP_GET_LOCAL:
		case LUSP_VMOP_SET_LOCAL:
			code = compile_getset_local(code, op);
			break;
			
		case LUSP_VMOP_GET_GLOBAL:
		case LUSP_VMOP_SET_GLOBAL:
			code = compile_getset_global(code, op);
			break;
			
		case LUSP_VMOP_PUSH:
			code = compile_push(code, op);
			break;
			
		case LUSP_VMOP_BIND:
			code = compile_bind(code, op);
			break;
			
		case LUSP_VMOP_BIND_REST:
			DL_ASSERT(false); // not supported yet
			break;
			
		case LUSP_VMOP_UNBIND:
			code = compile_unbind(code, op);
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
			code = compile_create_closure(code, op);
			break;
			
		default:
			DL_ASSERT(false);
		}
	}
	
	// second pass: fixup labels
	for (unsigned int i = 0; i < count; ++i)
	{
		struct lusp_vm_op_t op = ops[i];
		
		if (op.opcode != LUSP_VMOP_JUMP && op.opcode != LUSP_VMOP_JUMP_IFNOT) continue;
		
		LABEL32(jumps[i], labels[op.jump.index]);
	}
}

void lusp_compile_jit(struct lusp_vm_bytecode_t* code)
{
	if (code->jit) return;
	
	code->jit = allocate_code();
	
	compile((unsigned char*)code->jit, code->env, code->ops, code->count);
	
	code->evaluator = (lusp_vm_evaluator_t)code->jit;
}