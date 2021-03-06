#include "write.h"

#include "object.h"

#include <stdio.h>

static inline void lusp_write_string(struct lusp_object_t object)
{
	putc('"', stdout);

	for (const char* value = object.string; *value; ++value)
	{
		if (*value == '\\' || *value == '"') putc('\\', stdout);
		putc(*value, stdout);
	}

	putc('"', stdout);
}

static inline void lusp_write_cons(struct lusp_object_t object)
{
	printf("(");

	// first element
	lusp_write(object.cons[0]);
	object = object.cons[1];

	// remaining list elements
	while (object.type == LUSP_OBJECT_CONS)
	{
		printf(" ");
		lusp_write(object.cons[0]);
		object = object.cons[1];
	}

	// dotted pair
	if (object.type != LUSP_OBJECT_NULL)
	{
		printf(" . ");
		lusp_write(object);
	}

	printf(")");
}

void lusp_write(struct lusp_object_t object)
{
	switch (object.type)
	{
	case LUSP_OBJECT_NULL:
		printf("()");
		break;

	case LUSP_OBJECT_SYMBOL:
		printf("%s", object.symbol->name);
		break;

	case LUSP_OBJECT_BOOLEAN:
		printf(object.boolean ? "#t" : "#f");
		break;

	case LUSP_OBJECT_INTEGER:
		printf("%d", object.integer);
		break;

	case LUSP_OBJECT_REAL:
		printf("%f", object.real);
		break;

	case LUSP_OBJECT_STRING:
		lusp_write_string(object);
		break;

	case LUSP_OBJECT_CONS:
		lusp_write_cons(object);
		break;

	case LUSP_OBJECT_CLOSURE:
		printf("#<closure:%p>", object.closure);
		break;

	case LUSP_OBJECT_FUNCTION:
		printf("#<function:%p>", object.function);
		break;

	case LUSP_OBJECT_OBJECT:
		printf("#<object:%p>", object.object);
		break;

	default:
		printf("#<unknown>");
	}
}
