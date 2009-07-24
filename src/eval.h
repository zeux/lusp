// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_object_t;

struct lusp_object_t* lusp_eval(struct lusp_object_t* object);

struct lusp_vm_bytecode_t;

void lusp_bytecode_setup(struct lusp_vm_bytecode_t* code);

