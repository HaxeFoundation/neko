/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include "context.h"

#ifdef __WIN32
#include <windows.h>

_context *context_new() {
	DWORD t = TlsAlloc();
	TlsSetValue(t,NULL);
	return (_context*)t;
}

void context_delete( _context *ctx ) {
	TlsFree((DWORD)ctx);
}

void context_set( _context *ctx, void *c ) {
	TlsSetValue((DWORD)ctx,c);
}

void *context_get( _context *ctx ) {
	if( ctx == NULL )
		return NULL;
	return (void*)TlsGetValue((DWORD)ctx);
}

#else
/* ************************************************************************ */
#include "objtable.h"
#include <malloc.h>

struct _context {
	void *data;
};

_context *context_new() {
	_context *c = malloc(sizeof(_context));
	c->data = NULL;
	return c;
}

void context_delete( _context *ctx ) {
	free(ctx);
}

void context_set( _context *ctx, void *data ) {
	ctx->data = data;
}

void *context_get( _context *ctx ) {
	if( ctx == NULL )
		return NULL;
	return ctx->data;
}

#endif
/* ************************************************************************ */
