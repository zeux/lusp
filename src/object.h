// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_vm_bytecode_t;
struct lusp_vm_closure_t;
struct lusp_environment_t;

enum lusp_object_type_t
{
	LUSP_OBJECT_SYMBOL,
	LUSP_OBJECT_BOOLEAN,
	LUSP_OBJECT_INTEGER,
	LUSP_OBJECT_REAL,
	LUSP_OBJECT_STRING,
	LUSP_OBJECT_CONS,
	LUSP_OBJECT_CLOSURE,
	LUSP_OBJECT_PROCEDURE
};

struct lusp_object_t;

typedef struct lusp_object_t* (*lusp_procedure_t)(struct lusp_environment_t* env, struct lusp_object_t** args, unsigned int count);

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
			struct lusp_object_t* next;
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
			struct lusp_vm_closure_t* closure;
			struct lusp_vm_bytecode_t* code;
		} closure;
		
		struct
		{
		    lusp_procedure_t code;
		} procedure;
	};
};

bool lusp_internal_object_init();
void lusp_internal_object_term();

struct lusp_object_t* lusp_mksymbol(const char* name);
struct lusp_object_t* lusp_mkboolean(bool value);
struct lusp_object_t* lusp_mkinteger(int value);
struct lusp_object_t* lusp_mkreal(float value);
struct lusp_object_t* lusp_mkstring(const char* value);
struct lusp_object_t* lusp_mkcons(struct lusp_object_t* car, struct lusp_object_t* cdr);
struct lusp_object_t* lusp_mkclosure(struct lusp_vm_bytecode_t* code, unsigned int upref_count);
struct lusp_object_t* lusp_mkprocedure(lusp_procedure_t code);