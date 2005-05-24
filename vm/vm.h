/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#pragma once
#include <setjmp.h>
#include "neko.h"
#include "context.h"

typedef struct _neko_vm neko_vm;
typedef void (*printer)( const char *, int );

struct _neko_vm {
	int *spmin;
	int *spmax;
	int *sp;
	int *csp;
	int *trap;
	objtable fields;
	value env;
	value val_this;
	const char *error;
	printer print;
	jmp_buf start;
	char tmp[100];
};

extern int *callback_return;
extern _context *vm_context;

#define NEKO_VM()	((neko_vm*)context_get(vm_context))

extern value interp( neko_vm *vm, int acc, int *pc, value env );

/* ************************************************************************ */
