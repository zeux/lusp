// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#include <lusp/compiler/internal.h>

static inline void emit(struct compiler_t* compiler, struct lusp_vm_op_t op)
{
	check(compiler, compiler->op_count < sizeof(compiler->ops) / sizeof(compiler->ops[0]), "op buffer overflow");
	
	compiler->ops[compiler->op_count++] = op;
}

static inline void emit_get_object(struct compiler_t* compiler, struct lusp_object_t* object)
{
    struct lusp_vm_op_t op;
    op.opcode = LUSP_VMOP_GET_OBJECT;
    op.get_object.object = object;
    emit(compiler, op);
}

static inline void emit_getset_local(struct compiler_t* compiler, bool set, unsigned int index)
{
    struct lusp_vm_op_t op;
    op.opcode = set ? LUSP_VMOP_SET_LOCAL : LUSP_VMOP_GET_LOCAL;
    op.getset_local.index = index;
    emit(compiler, op);
}

static inline void emit_getset_upval(struct compiler_t* compiler, bool set, unsigned int index)
{
    struct lusp_vm_op_t op;
    op.opcode = set ? LUSP_VMOP_SET_UPVAL : LUSP_VMOP_GET_UPVAL;
    op.getset_local.index = index;
    emit(compiler, op);
}

static inline void emit_getset_global(struct compiler_t* compiler, bool set, struct lusp_environment_slot_t* slot)
{
    struct lusp_vm_op_t op;
    op.opcode = set ? LUSP_VMOP_SET_GLOBAL : LUSP_VMOP_GET_GLOBAL;
    op.getset_global.slot = slot;
    emit(compiler, op);
}

static inline void emit_push(struct compiler_t* compiler)
{
    compiler->current_temp_count++;
    compiler->temp_count = max(compiler->temp_count, compiler->current_temp_count);
    
    struct lusp_vm_op_t op;
    op.opcode = LUSP_VMOP_PUSH;
    emit(compiler, op);
}

static inline void emit_call(struct compiler_t* compiler, unsigned int count)
{
    DL_ASSERT(compiler->current_temp_count >= count);
    compiler->current_temp_count -= count;
    
    struct lusp_vm_op_t op;
    op.opcode = LUSP_VMOP_CALL;
    op.call.count = count;
    emit(compiler, op);
}

static inline void emit_return(struct compiler_t* compiler)
{
    struct lusp_vm_op_t op;
    op.opcode = LUSP_VMOP_RETURN;
    emit(compiler, op);
}

static inline void emit_jump(struct compiler_t* compiler, enum lusp_vm_opcode_t opcode, int offset)
{
    DL_ASSERT(opcode == LUSP_VMOP_JUMP || opcode == LUSP_VMOP_JUMP_IF || opcode == LUSP_VMOP_JUMP_IFNOT);
    struct lusp_vm_op_t op;
    op.opcode = opcode;
    op.jump.offset = offset;
    emit(compiler, op);
}

static inline void emit_create_closure(struct compiler_t* compiler, struct lusp_vm_bytecode_t* bytecode)
{
    struct lusp_vm_op_t op;
    op.opcode = LUSP_VMOP_CREATE_CLOSURE;
    op.create_closure.code = bytecode;
    emit(compiler, op);
}

static inline void emit_create_list(struct compiler_t* compiler, unsigned int index)
{
    struct lusp_vm_op_t op;
    op.opcode = LUSP_VMOP_CREATE_LIST;
    op.create_list.index = index;
    emit(compiler, op);
}

static inline void fixup_jump(struct compiler_t* compiler, unsigned int jump, unsigned int dest)
{
    struct lusp_vm_op_t* op = &compiler->ops[jump];
    
    DL_ASSERT(op->opcode == LUSP_VMOP_JUMP || op->opcode == LUSP_VMOP_JUMP_IF || op->opcode == LUSP_VMOP_JUMP_IFNOT);
	op->jump.offset = (int)dest - (int)jump;
}
