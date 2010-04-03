// DeepLight Engine (c) Zeux 2006-2010

#pragma once

#include <lusp/compiler/internal.h>

static inline void emit(struct compiler_t* compiler, struct lusp_vm_op_t op, enum lusp_vm_opcode_t opcode, unsigned int reg)
{
	DL_ASSERT(compiler->op_count < countof(compiler->ops));
	
	op.opcode = (uint8_t)opcode;
	op.padding = 0;
	op.reg = (uint16_t)reg;
	
	compiler->ops[compiler->op_count++] = op;
}

static inline void emit_load_const(struct compiler_t* compiler, unsigned int reg, struct lusp_object_t* object)
{
    struct lusp_vm_op_t op;
    op.load_const.object = object;
    emit(compiler, op, LUSP_VMOP_LOAD_CONST, reg);
}

static inline void emit_loadstore_global(struct compiler_t* compiler, unsigned int reg, struct lusp_environment_slot_t* slot, bool store)
{
    struct lusp_vm_op_t op;
    op.loadstore_global.slot = slot;
    emit(compiler, op, store ? LUSP_VMOP_STORE_GLOBAL : LUSP_VMOP_LOAD_GLOBAL, reg);
}

static inline void emit_loadstore_upval(struct compiler_t* compiler, unsigned int reg, unsigned int index, bool store)
{
    struct lusp_vm_op_t op;
    op.loadstore_upval.index = index;
    emit(compiler, op, store ? LUSP_VMOP_STORE_UPVAL : LUSP_VMOP_LOAD_UPVAL, reg);
}
static inline void emit_move(struct compiler_t* compiler, unsigned int reg, unsigned int index)
{
    struct lusp_vm_op_t op;
    op.move.index = index;
    emit(compiler, op, LUSP_VMOP_MOVE, reg);
}

static inline void emit_call(struct compiler_t* compiler, unsigned int reg, unsigned int args, unsigned int count)
{
    struct lusp_vm_op_t op;
    op.call.args = (uint16_t)args;
    op.call.count = (uint16_t)count;
    emit(compiler, op, LUSP_VMOP_CALL, reg);
}

static inline void emit_return(struct compiler_t* compiler, unsigned int reg)
{
    struct lusp_vm_op_t op;
    op.dummy = 0;
    emit(compiler, op, LUSP_VMOP_RETURN, reg);
}

static inline void emit_jump(struct compiler_t* compiler, int offset)
{
    struct lusp_vm_op_t op;
    op.jump.offset = offset;
    emit(compiler, op, LUSP_VMOP_JUMP, 0);
}

static inline void emit_jump_if(struct compiler_t* compiler, unsigned int reg, int offset)
{
    struct lusp_vm_op_t op;
    op.jump.offset = offset;
    emit(compiler, op, LUSP_VMOP_JUMP_IF, reg);
}

static inline void emit_jump_ifnot(struct compiler_t* compiler, unsigned int reg, int offset)
{
    struct lusp_vm_op_t op;
    op.jump.offset = offset;
    emit(compiler, op, LUSP_VMOP_JUMP_IFNOT, reg);
}

static inline void emit_create_closure(struct compiler_t* compiler, unsigned int reg, struct lusp_vm_bytecode_t* bytecode)
{
    struct lusp_vm_op_t op;
    op.create_closure.code = bytecode;
    emit(compiler, op, LUSP_VMOP_CREATE_CLOSURE, reg);
}

static inline void emit_close(struct compiler_t* compiler, unsigned int begin)
{
	struct lusp_vm_op_t op;
	op.close.begin = begin;
	emit(compiler, op, LUSP_VMOP_CLOSE, 0);
}

static inline void emit_binop(struct compiler_t* compiler, enum lusp_vm_opcode_t opcode, unsigned int reg, unsigned int left, unsigned int right)
{
	DL_ASSERT(opcode == LUSP_VMOP_ADD || opcode == LUSP_VMOP_SUBTRACT || opcode == LUSP_VMOP_MULTIPLY ||
		opcode == LUSP_VMOP_DIVIDE || opcode == LUSP_VMOP_MODULO || opcode == LUSP_VMOP_EQUAL ||
		opcode == LUSP_VMOP_NOT_EQUAL || opcode == LUSP_VMOP_LESS || opcode == LUSP_VMOP_LESS_EQUAL ||
		opcode == LUSP_VMOP_GREATER || opcode == LUSP_VMOP_GREATER_EQUAL);
		
	struct lusp_vm_op_t op;
	op.binop.left = (uint16_t)left;
	op.binop.right = (uint16_t)right;
	emit(compiler, op, opcode, reg);
}

static inline void fixup_jump(struct compiler_t* compiler, unsigned int jump, unsigned int dest)
{
    struct lusp_vm_op_t* op = &compiler->ops[jump];
    
    DL_ASSERT(op->opcode == LUSP_VMOP_JUMP || op->opcode == LUSP_VMOP_JUMP_IF || op->opcode == LUSP_VMOP_JUMP_IFNOT);
	op->jump.offset = (int)dest - (int)jump - 1;
}
