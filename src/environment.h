// DeepLight Engine (c) Zeux 2006-2009

#pragma once

#include <lusp/object.h>

struct lusp_environment_slot_t
{
	struct lusp_object_t name;
	struct lusp_object_t value;
	
	struct lusp_environment_slot_t* next;
};

struct lusp_environment_t
{
	struct lusp_environment_slot_t* head;
};

struct lusp_environment_t* lusp_environment_create();

struct lusp_environment_slot_t* lusp_environment_get_slot(struct lusp_environment_t* env, struct lusp_object_t name);

struct lusp_object_t lusp_environment_get(struct lusp_environment_t* env, struct lusp_object_t name);
void lusp_environment_put(struct lusp_environment_t* env, struct lusp_object_t name, struct lusp_object_t object);
