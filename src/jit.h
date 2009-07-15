// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_vm_bytecode_t;

void lusp_compile_jit(struct lusp_vm_bytecode_t* code);
struct lusp_object_t* lusp_eval_jit(struct lusp_vm_bytecode_t* code);
