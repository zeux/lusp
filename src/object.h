// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_vm_bytecode_t;
struct lusp_vm_closure_t;
struct lusp_environment_t;

enum lusp_object_type_t
{
    LUSP_OBJECT_NULL,
	LUSP_OBJECT_SYMBOL,
	LUSP_OBJECT_BOOLEAN,
	LUSP_OBJECT_INTEGER,
	LUSP_OBJECT_REAL,
	LUSP_OBJECT_STRING,
	LUSP_OBJECT_CONS,
	LUSP_OBJECT_CLOSURE,
	LUSP_OBJECT_FUNCTION,
	LUSP_OBJECT_OBJECT,
	LUSP_OBJECT_CALL_FRAME,
};

struct lusp_symbol_t
{
	const char* name;
	struct lusp_symbol_t* next;
};

struct lusp_object_t
{
	enum lusp_object_type_t type;
	
	union
	{
		struct lusp_symbol_t* symbol;
		bool boolean;
		int integer;
		float real;
		const char* string;
		struct lusp_object_t* cons;
		struct lusp_vm_closure_t* closure;
		void* function;
		void* object;
		char call_frame[1];
	};
};

typedef struct lusp_object_t (*lusp_function_t)(struct lusp_environment_t* env, struct lusp_object_t* args, unsigned int count);

bool lusp_object_init();
void lusp_object_term();

struct lusp_object_t lusp_mknull();
struct lusp_object_t lusp_mksymbol(const char* name);
struct lusp_object_t lusp_mkboolean(bool value);
struct lusp_object_t lusp_mkinteger(int value);
struct lusp_object_t lusp_mkreal(float value);
struct lusp_object_t lusp_mkstring(const char* value);
struct lusp_object_t lusp_mkcons(struct lusp_object_t car, struct lusp_object_t cdr);
struct lusp_object_t lusp_mkclosure(struct lusp_vm_bytecode_t* code, unsigned int upval_count);
struct lusp_object_t lusp_mkfunction(lusp_function_t code);
struct lusp_object_t lusp_mkobject(void* object);