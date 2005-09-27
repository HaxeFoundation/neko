/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */
#ifndef _NEKO_VMCONTEXT_H
#define _NEKO_VMCONTEXT_H
#include <setjmp.h>
#include "neko.h"
#include "context.h"

typedef void (*printer)( const char *, int );

#define INIT_STACK_SIZE (1 << 7)
#define MAX_STACK_SIZE	(1 << 18)

struct _neko_vm {
	int *spmin;
	int *spmax;
	int *sp;
	int *csp;
	int *trap;
	int ncalls;
	value env;
	value this;
	printer print;
	jmp_buf start;
	char tmp[100];
	void *custom;
};

#ifndef neko_vm_def
typedef struct _neko_vm neko_vm;
#define neko_vm_def
#endif

extern int *callback_return;
extern _context *neko_vm_context;

#define NEKO_VM()	((neko_vm*)context_get(neko_vm_context))

extern value neko_interp( neko_vm *vm, int acc, int *pc, value env );

#endif
/* ************************************************************************ */
