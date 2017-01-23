#pragma once

#include "object.h"
#include "memory.h"
#include "bytecode.h"

static inline struct lusp_vm_upval_t* mkupval(struct lusp_vm_upval_t** list, struct lusp_object_t* ref)
{
    // look for ref in list
    for (struct lusp_vm_upval_t* upval = *list; upval; upval = upval->next)
        if (upval->ref == ref)
            return upval;
            
    // create new upval
    struct lusp_vm_upval_t* result = (struct lusp_vm_upval_t*)lusp_memory_allocate(sizeof(struct lusp_vm_upval_t));
    assert(result);
    
    result->ref = ref;
    result->next = *list;
    *list = result;
    
    return result;
}

static inline struct lusp_vm_upval_t* close_upvals(struct lusp_vm_upval_t* list, struct lusp_object_t* begin)
{
    while (list->ref >= begin)
    {
        struct lusp_vm_upval_t* next = list->next;
        
        list->object = *list->ref;
        list->ref = &list->object;
        
        list = next;
    }
    
    return list;
}

#define ARITH(name, op) \
	static inline struct lusp_object_t name(struct lusp_object_t* left, struct lusp_object_t* right) \
	{ \
		return lusp_mkinteger(left->integer op right->integer); \
	}
	
ARITH(binop_add, +);
ARITH(binop_subtract, -);
ARITH(binop_multiply, *);
ARITH(binop_divide, /);
ARITH(binop_modulo, %); 

#undef ARITH

#define COMP(name, op) \
	static inline struct lusp_object_t name(struct lusp_object_t* left, struct lusp_object_t* right) \
	{ \
		return lusp_mkboolean(left->integer op right->integer); \
	}
	
COMP(binop_equal, ==);
COMP(binop_not_equal, !=);
COMP(binop_less, <);
COMP(binop_less_equal, <=);
COMP(binop_greater, >);
COMP(binop_greater_equal, >=);

#undef COMP
