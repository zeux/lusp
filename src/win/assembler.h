// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#define CODE() code
#define EMIT8(value) *code++ = value
#define EMIT32(value) *(uint32_t*)code = (uint32_t)(uintptr_t)(value), code += sizeof(uint32_t)

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

#define RET() EMIT8(0xc3)

#define MOV_REG_IMM32(reg, value) EMIT8(0xb8 + reg), EMIT32(value)
#define MOV_REG_PIMM(reg, addr) (reg == EAX ? EMIT8(0xa1) : (EMIT8(0x3e), EMIT8(0x8b), EMIT8((reg << 3) + 5))), EMIT32(addr)
#define MOV_PIMM_REG(addr, reg) (reg == EAX ? EMIT8(0xa3) : (EMIT8(0x3e), EMIT8(0x89), EMIT8((reg << 3) + 5))), EMIT32(addr)
#define MOV_REG_REG(reg1, reg2) EMIT8(0x8b), EMIT8(0xc0 + (reg1 << 3) + reg2)
#define MOV_PREG_REG(reg1, reg2) EMIT8(0x89), EMIT8((reg2 << 3) + reg1)
#define MOV_PREG_OFF8_REG(reg1, offset, reg2) EMIT8(0x89), EMIT8(0x40 + (reg2 << 3) + reg1), EMIT8(offset)
#define MOV_PREG_OFF32_REG(reg1, offset, reg2) EMIT8(0x89), EMIT8(0x80 + (reg2 << 3) + reg1), EMIT32(offset)
#define MOV_REG_PREG(reg1, reg2) EMIT8(0x8b), EMIT8((reg1 << 3) + reg2)
#define MOV_REG_PREG_OFF8(reg1, reg2, offset) EMIT8(0x8b), EMIT8(0x40 + (reg1 << 3) + reg2), (reg2 == ESP ? EMIT8(0x24) : (void)0), EMIT8(offset)
#define MOV_REG_PREG_OFF32(reg1, reg2, offset) EMIT8(0x8b), EMIT8(0x80 + (reg1 << 3) + reg2), EMIT32(offset)
#define MOV_PREG_OFF8_IMM32(reg, offset, value) EMIT8(0xc7), EMIT8(0x40 + reg), EMIT8(offset), EMIT32(value)

#define LEA_REG_PREG_OFF32(reg1, reg2, offset) EMIT8(0x8d), EMIT8(0x80 + (reg1 << 3) + reg2), (reg2 == ESP ? EMIT8(0x24) : (void)0), EMIT32(offset)

#define ADD_REG_IMM8(reg, value) EMIT8(0x83), EMIT8(0xc0 + reg), EMIT8(value)
#define ADD_REG_IMM32(reg, value) (reg == EAX ? EMIT8(0x05) : (EMIT8(0x81), EMIT8(0xc0 + reg))), EMIT32(value)
#define ADD_REG_REG(reg1, reg2) EMIT8(0x03), EMIT8(0xc0 + (reg1 << 3) + reg2)
#define SUB_REG_IMM8(reg, value) EMIT8(0x83), EMIT8(0xe8 + reg), EMIT8(value)
#define SUB_REG_IMM32(reg, value) (reg == EAX ? EMIT8(0x2d) : (EMIT8(0x81), EMIT8(0xe8 + reg))), EMIT32(value)
#define SUB_REG_REG(reg1, reg2) EMIT8(0x2b), EMIT8(0xc0 + (reg1 << 3) + reg2)

#define SHL_REG_IMM8(reg, value) EMIT8(0xc1), EMIT8(0xe0 + reg), EMIT8(value)

#define PUSH_IMM8(value) EMIT8(0x6a), EMIT8(value)
#define PUSH_IMM32(value) EMIT8(0x68), EMIT32(value)
#define PUSH_REG(reg) EMIT8(0x50 + reg)
#define PUSH_PREG(reg) EMIT8(0xff), EMIT8(0x30 + reg), (reg == ESP ? EMIT8(0x24) : (void)0)
#define PUSH_PREG_OFF8(reg, offset) EMIT8(0xff), EMIT8(0x70 + reg), (reg == ESP ? EMIT8(0x24) : (void)0), EMIT8(offset)
#define POP_REG(reg) EMIT8(0x58 + reg)

#define JMP_IMM32(offset) EMIT8(0xe9), EMIT32(offset)
#define JMP_FUNC(func) JMP_IMM32((uint8_t*)func - CODE() - 4)

#define JE_IMM32(offset) EMIT8(0x0f), EMIT8(0x84), EMIT32(offset)
#define JNE_IMM32(offset) EMIT8(0x0f), EMIT8(0x85), EMIT32(offset)

#define JMP_IMM8(label) EMIT8(0xeb), label = CODE(), EMIT8(0)
#define JZ_IMM8(label) EMIT8(0x74), label = CODE(), EMIT8(0)
#define JNE_IMM8(label) EMIT8(0x75), label = CODE(), EMIT8(0)

#define LABEL8(label) *(uint8_t*)label = (uint8_t)(uintptr_t)(CODE() - label - 1)
#define LABEL32(label, code) *(uint32_t*)label = (uint32_t)(uintptr_t)(code - label - 4)

#define CMP_REG_IMM8(reg, value) EMIT8(0x83), EMIT8(0xF8 + reg), EMIT8(value)
#define CMP_PREG_IMM8(reg, value) EMIT8(0x83), EMIT8(0x38 + reg), EMIT8(value)
#define CMP_PREG_OFF8_IMM8(reg, offset, value) EMIT8(0x83), EMIT8(0x78 + reg), EMIT8(offset), EMIT8(value)

#define CALL_REG(reg) EMIT8(0xff), EMIT8(0xd0 + reg)
#define CALL_IMM32(offset) EMIT8(0xe8), EMIT32(offset)
#define CALL_FUNC(func) CALL_IMM32((uint8_t*)func - CODE() - 4)