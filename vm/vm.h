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

typedef void (*printer)( const char *, int );

#define INIT_STACK_SIZE (1 << 7)
#define MAX_STACK_SIZE	(1 << 18)
#define PROF_SIZE		(1 << 16)
#define CALL_MAX_ARGS	5

struct _neko_vm {
	int_val *sp;
	int_val *csp;
	value env;
	value vthis;
	int_val *spmin;
	int_val *spmax;
	int_val trap;
	jmp_buf start;
	int ncalls;
	value exc_stack;
	printer print;
	void *custom;
	char tmp[100];
};

extern int_val *callback_return;
extern _context *neko_vm_context;

#define NEKO_VM()	((neko_vm*)context_get(neko_vm_context))

extern value alloc_apply( int nargs, value env );
extern value neko_interp( neko_vm *vm, void *m, int_val acc, int_val *pc );

#endif
/* ************************************************************************ */
