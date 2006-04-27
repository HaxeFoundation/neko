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
#include "context.h"

#ifdef _WIN32
#include <windows.h>
// disable warnings for type conversions
#pragma warning(disable : 4311)
#pragma warning(disable : 4312)

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
#include <stdlib.h>

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
