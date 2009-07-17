// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#define CODE() code
#define EMIT8(value) *code++ = value
#define EMIT32(value) *(uint32_t*)code = (uint32_t)(uintptr_t)(value), code += sizeof(uint32_t)

#define RET() EMIT8(0xc3)

#define MOV_EAX_IMM32(value) EMIT8(0xb8), EMIT32(value)

#define MOV_ADDR_EAX(addr) EMIT8(0xa3), EMIT32(addr)

#define MOV_EAX_ADDR(addr) EMIT8(0xa1), EMIT32(addr)
#define MOV_ECX_ADDR(addr) EMIT8(0x3e), EMIT8(0x8b), EMIT8(0x0d), EMIT32(addr)

#define MOV_PECX_EBX() EMIT8(0x89), EMIT8(0x19)
#define MOV_PEDX_EAX() EMIT8(0x89), EMIT8(0x02)

#define MOV_EBX_ECX() EMIT8(0x8b), EMIT8(0xd9)

#define MOV_EBX_PEBX() EMIT8(0x8b), EMIT8(0x1b)
#define MOV_ECX_PEBX() EMIT8(0x8b), EMIT8(0x0b)
#define MOV_ECX_PECX() EMIT8(0x8b), EMIT8(0x09)

#define MOV_EAX_PEBX_OFF32(offset) EMIT8(0x8b), EMIT8(0x83), EMIT32(offset)
#define MOV_EAX_PECX_OFF32(offset) EMIT8(0x8b), EMIT8(0x81), EMIT32(offset)
#define MOV_ESI_PEDX_OFF32(offset) EMIT8(0x8b), EMIT8(0xb2), EMIT32(offset)

#define MOV_PEBX_OFF32_EAX(offset) EMIT8(0x89), EMIT8(0x83), EMIT32(offset)
#define MOV_PECX_OFF32_EAX(offset) EMIT8(0x89), EMIT8(0x81), EMIT32(offset)
#define MOV_PECX_OFF32_ESI(offset) EMIT8(0x89), EMIT8(0xb1), EMIT32(offset)

#define MOV_PEAX_IMM32(value) EMIT8(0xc7), EMIT8(0x00), EMIT32(value)

#define MOV_PEAX_OFF8_EBX(offset) EMIT8(0x89), EMIT8(0x58), EMIT8(offset)

#define MOV_PEAX_OFF8_IMM32(offset, value) EMIT8(0xc7), EMIT8(0x40), EMIT8(offset), EMIT32(value)
#define MOV_PECX_OFF8_IMM32(offset, value) EMIT8(0xc7), EMIT8(0x41), EMIT8(offset), EMIT32(value)

#define MOV_EAX_PEAX_OFF8(offset) EMIT8(0x8b), EMIT8(0x40), EMIT8(offset)
#define MOV_EBX_PEAX_OFF8(offset) EMIT8(0x8b), EMIT8(0x58), EMIT8(offset)

#define JMP_IMM32(offset) EMIT8(0xe9), EMIT32(offset)

#define JE_IMM32(offset) EMIT8(0x0f), EMIT8(0x84), EMIT32(offset)

#define JMP_IMM8(label) EMIT8(0xeb), label = CODE(), EMIT8(0)
#define JZ_IMM8(label) EMIT8(0x74), label = CODE(), EMIT8(0)
#define JNE_IMM8(label) EMIT8(0x75), label = CODE(), EMIT8(0)

#define LABEL8(label) *(uint8_t*)label = (uint8_t)(uintptr_t)(CODE() - label - 1)
#define LABEL32(label, code) *(uint32_t*)label = (uint32_t)(uintptr_t)(code - label - 4)

#define TEST_EAX_EAX() EMIT8(0x85), EMIT8(0xc0)

#define CMP_PEAX_IMM8(value) EMIT8(0x83), EMIT8(0x38), EMIT8(value)

#define CMP_PEAX_OFF8_IMM8(offset, value) EMIT8(0x83), EMIT8(0x78), EMIT8(offset), EMIT8(value)

#define CALL_EAX() EMIT8(0xff), EMIT8(0xd0)

#define PUSH_IMM32(value) EMIT8(0x68), EMIT32(value)

#define PUSH_EBX() EMIT8(0x53)
#define PUSH_EDX() EMIT8(0x52)
#define POP_EBX() EMIT8(0x5b)
#define POP_EDX() EMIT8(0x5a)
#define POP_ECX() EMIT8(0x59)

#define ADD_EDX_IMM8(value) EMIT8(0x83), EMIT8(0xc2), EMIT8(value)

#define ADD_ADDR_IMM32(addr, value) EMIT8(0x3e), EMIT8(0x81), EMIT8(0x05), EMIT32(addr), EMIT32(value)

#define SUB_EDX_IMM32(value) EMIT8(0x81), EMIT8(0xea), EMIT32(value)