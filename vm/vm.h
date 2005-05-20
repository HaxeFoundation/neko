/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#pragma once
#include "neko.h"
#include "context.h"

typedef struct _neko_vm neko_vm;
typedef void (*printer)( const char *, int );

struct _neko_vm {
	int *spmin;
	int *spmax;
	int *sp;
	int *csp;
	value val_this;
	const char *error;
	printer print;
	char tmp[100];
};

extern _context *vm_context;

#define NEKO_VM()	((neko_vm*)context_get(vm_context))

/* ************************************************************************ */
