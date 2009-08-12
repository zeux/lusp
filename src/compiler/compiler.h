// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_ast_node_t;
struct lusp_object_t;
struct lusp_environment_t;

struct lusp_object_t lusp_compile(struct lusp_environment_t* env, struct lusp_ast_node_t* node);
