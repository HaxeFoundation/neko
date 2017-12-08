/*
 * Copyright (C)2005-2017 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _NEKO_VMCONTEXT_H
#define _NEKO_VMCONTEXT_H
#include <setjmp.h>
#include "neko_vm.h"

#define INIT_STACK_SIZE (1 << 8)
#define MAX_STACK_SIZE	(1 << 18)
#define MAX_STACK_PER_FUNCTION	128
#define PROF_SIZE		(1 << 20)
#define CALL_MAX_ARGS	5
#define NEKO_FIELDS_MASK 63

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
extern mt_local *neko_vm_context;

#define NEKO_VM()	((neko_vm*)local_get(neko_vm_context))

extern value neko_alloc_apply( int nargs, value env );
extern value neko_interp( neko_vm *vm, void *m, int_val acc, int_val *pc );
extern int_val *neko_get_ttable();

#endif
/* ************************************************************************ */
