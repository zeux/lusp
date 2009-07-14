// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_vm_environment_t;
struct lusp_vm_bytecode_t;

enum lusp_object_type_t
{
	LUSP_OBJECT_SYMBOL,
	LUSP_OBJECT_BOOLEAN,
	LUSP_OBJECT_INTEGER,
	LUSP_OBJECT_REAL,
	LUSP_OBJECT_STRING,
	LUSP_OBJECT_CONS,
	LUSP_OBJECT_CLOSURE
};

struct lusp_object_t
{
	enum lusp_object_type_t type;
	
	union
	{
		struct
		{
			bool value;
		} boolean;
		
		struct
		{
			const char* name;
		} symbol;
		
		struct
		{
			int value;
		} integer;
		
		struct
		{
			float value;
		} real;
		
		struct
		{
			const char* value;
		} string;
		
		struct
		{
			struct lusp_object_t* car;
			struct lusp_object_t* cdr;
		} cons;
		
		struct
		{
			struct lusp_vm_environment_t* env;
			struct lusp_vm_bytecode_t* code;
		} closure;
	};
};

struct lusp_object_t* lusp_mksymbol(const char* name);
struct lusp_object_t* lusp_mkboolean(bool value);
struct lusp_object_t* lusp_mkinteger(int value);
struct lusp_object_t* lusp_mkreal(float value);
struct lusp_object_t* lusp_mkstring(const char* value);
struct lusp_object_t* lusp_mkcons(struct lusp_object_t* car, struct lusp_object_t* cdr);