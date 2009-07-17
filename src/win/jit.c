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

void* allocate_code()
{
	void* result = mem_arena_allocate(&g_lusp_heap, 16*1024, 4096);
	DL_ASSERT(result);
	
	DWORD flags;
	VirtualProtect(result, 16*1024, PAGE_EXECUTE_READWRITE, &flags);
	
	return result;
}

void compile(unsigned char* code, struct lusp_environment_t* env, struct lusp_vm_op_t* ops, unsigned int count)
{
	// registers:
	// eax: value
	// ebx: bind_frame
	// ecx: used for internal calculations
	
	unsigned char* labels[1024];
	unsigned char* jumps[1024];
	
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
			// mov eax, imm32
			MOV_EAX_IMM32(op.get_object.object);
			break;
			
		case LUSP_VMOP_GET_LOCAL:
		case LUSP_VMOP_SET_LOCAL:
			if (op.getset_local.depth == 0)
			{
				// mov eax, dword ptr [ebx + offset]
				// mov dword ptr [ebx + offset], eax
				unsigned int offset = op.getset_local.index * 4 + 4;
				(op.opcode == LUSP_VMOP_GET_LOCAL) ? (MOV_EAX_PEBX_OFF32(offset)) : (MOV_PEBX_OFF32_EAX(offset));
			}
			else
			{
				// mov ecx, dword ptr [ebx]
				MOV_ECX_PEBX();
				
				// mov ecx, dword ptr [ecx]
				for (unsigned int i = 1; i < op.getset_local.depth; ++i) MOV_ECX_PECX();
				
				// mov eax, dword ptr [ecx + offset]
				// mov dword ptr [ecx + offset], eax
				unsigned int offset = op.getset_local.index * 4 + 4;
				(op.opcode == LUSP_VMOP_GET_LOCAL) ? (MOV_EAX_PECX_OFF32(offset)) : (MOV_PECX_OFF32_EAX(offset));
			}
			break;
			
		case LUSP_VMOP_GET_GLOBAL:
			// mov eax, slot->value
			MOV_EAX_ADDR(&op.getset_global.slot->value);
			break;
			
		case LUSP_VMOP_SET_GLOBAL:
			// mov slot->value, eax
			MOV_ADDR_EAX(&op.getset_global.slot->value);
			break;
			
		case LUSP_VMOP_PUSH:
			// mov dword ptr [edx], eax
			MOV_PEDX_EAX();
			
			// add edx, 4
			ADD_EDX_IMM8(4);
			break;
			
		case LUSP_VMOP_BIND:
			// mov ecx, heap.current
			MOV_ECX_ADDR(&g_lusp_heap.current);
			
			// add heap.current, 4 + count * 4
			ADD_ADDR_IMM32(&g_lusp_heap.current, 4 + op.bind.count * 4);
			
			// mov dword ptr [ecx], ebx
			MOV_PECX_EBX();
			
			// sub edx, count * 4
			SUB_EDX_IMM32(op.bind.count * 4);
			
			// mov esi, dword ptr [edx + offset]
			// mov dword ptr [ecx + offset], esi
			for (unsigned int j = 0; j < op.bind.count; ++j)
			{
				MOV_ESI_PEDX_OFF32(j * 4);
				MOV_PECX_OFF32_ESI(j * 4 + 4);
			}
			
			// mov ebx, ecx
			MOV_EBX_ECX();
			break;
			
		case LUSP_VMOP_BIND_REST:
			DL_ASSERT(false); // not supported yet
			break;
			
		case LUSP_VMOP_UNBIND:
			// mov ebx, dword ptr [ebx]
			MOV_EBX_PEBX();
			break;
			
		case LUSP_VMOP_CALL:
			// cmp dword ptr [eax], LUSP_OBJECT_PROCEDURE
			CMP_PEAX_IMM8(LUSP_OBJECT_PROCEDURE);
			
			// jne closure
			JNE_IMM8(30);
			
			// push count
			PUSH_IMM32(op.call.count);
			
			// sub edx, count * 4
			SUB_EDX_IMM32(op.call.count * 4);
			
			// push edx
			PUSH_EDX();
			
			// push env
			PUSH_IMM32(env);

			// mov eax, dword ptr [eax + 4]
			MOV_EAX_PEAX_OFF8(4);
			
			// call eax
			CALL_EAX();
			
			// pop edx
			POP_EDX();
			
			// pop edx
			POP_EDX();
			
			// pop ecx
			POP_ECX();
			
			// jmp end
			JMP_IMM32(13);
			
			// closure:
			// push ebx
			PUSH_EBX();
			
			// mov ebx, dword ptr [eax + 4]
			MOV_EBX_PEAX_OFF8(4);
			
			// mov eax, dword ptr [eax + 8]
			MOV_EAX_PEAX_OFF8(8);
			
			// mov eax, dword ptr [eax + 12]
			MOV_EAX_PEAX_OFF8(12);
			
			// call eax
			CALL_EAX();
			
			// pop ebx
			POP_EBX();
			
			// end:
			
			break;
			
		case LUSP_VMOP_RETURN:
			// ret
			RET();
			break;
			
		case LUSP_VMOP_JUMP:
			// jmp offset
			JMP_IMM32(0);
			jumps[i] = code - 4;
			break;
			
		case LUSP_VMOP_JUMP_IFNOT:
			// test eax, eax
			TEST_EAX_EAX();

			// jz skip
			JZ_IMM8(15);
			
			// cmp dword ptr [eax], LUSP_OBJECT_BOOLEAN
			CMP_PEAX_IMM8(LUSP_OBJECT_BOOLEAN);
			
			// jne skip
			JNE_IMM8(10);
			
			// cmp dword ptr [eax + 4], false
			CMP_PEAX_OFF8_IMM8(4, 0);
			
			// je label
			JE_IMM32(0);
			jumps[i] = code - 4;
			
			// skip:
			break;
			
		case LUSP_VMOP_CREATE_CLOSURE:
			// mov eax, heap.current
			MOV_EAX_ADDR(&g_lusp_heap.current);

			// add heap.current, sizeof(struct lusp_object_t)
			ADD_ADDR_IMM32(&g_lusp_heap.current, sizeof(struct lusp_object_t));
			
			// mov dword ptr [eax], LUSP_OBJECT_CLOSURE
			MOV_PEAX_IMM32(LUSP_OBJECT_CLOSURE);
			
			// mov dword ptr [eax + 4], ebx
			MOV_PEAX_OFF8_EBX(4);
			
			// mov dword ptr [eax + 8], op.create_closure.code
			MOV_PEAX_OFF8_IMM32(8, op.create_closure.code);
			
			// compile bytecode recursively
			lusp_compile_jit(op.create_closure.code);
			
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
		
		*(int*)jumps[i] = (int)(labels[op.jump.index] - jumps[i]) - 4;
	}
}

void lusp_compile_jit(struct lusp_vm_bytecode_t* code)
{
	if (code->jit) return;
	
	code->jit = allocate_code();
	
	compile((unsigned char*)code->jit, code->env, code->ops, code->count);
}

struct lusp_object_t* lusp_eval_jit(struct lusp_vm_bytecode_t* code)
{
	void* jit = code->jit;
	
	struct lusp_object_t* eval_stack[1024];
	
	// register setup
	__asm xor eax, eax;
	__asm xor ebx, ebx;
	__asm lea edx, eval_stack;
	
	// call function
	__asm mov ecx, jit;
	__asm call ecx;
}