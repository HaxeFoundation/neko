/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Motion-Twin											*/
/*																			*/
/* This library is free software; you can redistribute it and/or			*/
/* modify it under the terms of the GNU Lesser General Public				*/
/* License as published by the Free Software Foundation; either				*/
/* version 2.1 of the License, or (at your option) any later version.		*/
/*																			*/
/* This library is distributed in the hope that it will be useful,			*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU		*/
/* Lesser General Public License or the LICENSE file for more details.		*/
/*																			*/
/* ************************************************************************ */
#ifndef _NEKO_VMCONTEXT_H
#define _NEKO_VMCONTEXT_H
#include <setjmp.h>
#include "neko_vm.h"
#include "context.h"

#define INIT_STACK_SIZE (1 << 8)
#define MAX_STACK_SIZE	(1 << 18)
#define MAX_STACK_PER_FUNCTION	128
#define PROF_SIZE		(1 << 20)
#define CALL_MAX_ARGS	5

typedef struct _custom_list {
	vkind tag;
	void *custom;
	struct _custom_list *next;
} custom_list;

struct _neko_vm {
	int_val *sp;
	int_val *csp;
	value env;
	value vthis;
	int_val *spmin;
	int_val *spmax;
	int_val trap;
	void *jit_val;
	jmp_buf start;
	void *c_stack_max;
	int run_jit;
	value exc_stack;
	neko_printer print;
	void *print_param;
	custom_list *clist;
	value resolver;
	char tmp[100];
	int trusted_code;
	neko_stat_func fstats;
	neko_stat_func pstats;
};

extern int_val *callback_return;
extern _context *neko_vm_context;

#define NEKO_VM()	((neko_vm*)context_get(neko_vm_context))

extern value neko_alloc_apply( int nargs, value env );
extern value neko_interp( neko_vm *vm, void *m, int_val acc, int_val *pc );
extern int_val *neko_get_ttable();

#endif
/* ************************************************************************ */
