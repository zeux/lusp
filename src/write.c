// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/write.h>

#include <lusp/object.h>

#include <stdio.h>

void lusp_write(struct lusp_object_t* object)
{
	if (!object)
	{
		printf("#<null>");
		return;
	}
	
	switch (object->type)
	{
	case LUSP_OBJECT_SYMBOL:
		printf("%s", object->symbol.name);
		break;
		
	case LUSP_OBJECT_BOOLEAN:
		printf(object->boolean.value ? "#t" : "#f");
		break;
		
	case LUSP_OBJECT_INTEGER:
		printf("%d", object->integer.value);
		break;
		
	case LUSP_OBJECT_REAL:
		printf("%f", object->real.value);
		break;
		
	case LUSP_OBJECT_STRING:
		printf("\"%s\"", object->string.value); // TODO: escape output
		break;
		
	case LUSP_OBJECT_CONS:
		printf("(");
		lusp_write(object->cons.car);
		printf(" ");
		lusp_write(object->cons.cdr);
		printf(")");
		break;
		
	case LUSP_OBJECT_CLOSURE:
		printf("#<closure:%p>", object);
		break;
		
	case LUSP_OBJECT_PROCEDURE:
		printf("#<procedure:%p>", object);
		break;
		
	case LUSP_OBJECT_ENVIRONMENT:
		printf("#<environment:%p>", object);
		break;
		
	default:
		printf("#<unknown>");
	}
}