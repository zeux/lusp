// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#include <lusp/object.h>

void lusp_jit_set(bool enabled);
bool lusp_jit_get();

struct lusp_object_t lusp_eval(struct lusp_object_t object);