/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#pragma once
#include "neko.h"
#include "context.h"

#define CALLBACK_RETURN		0xFF000001

typedef struct _neko_vm neko_vm;

struct _neko_vm {
	int *spmin;
	int *spmax;
	int *sp;
	int *csp;
	value val_this;
	const char *error;
};

extern _context *vm_context;

#define NEKO_VM()	((neko_vm*)context_get(vm_context))

/* ************************************************************************ */
