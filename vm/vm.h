/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#ifndef _NEKO_VMCONTEXT_H
#define _NEKO_VMCONTEXT_H
#include <setjmp.h>
#include "neko.h"
#include "context.h"

typedef void (*printer)( const char *, int );

struct _neko_vm {
	int *spmin;
	int *spmax;
	int *sp;
	int *csp;
	int *trap;
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
