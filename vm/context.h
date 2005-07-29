/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#ifndef _NEKO_CONTEXT_H
#define _NEKO_CONTEXT_H

typedef struct _context _context;

_context *context_new();
void context_delete( _context *ctx );
void context_set( _context *ctx, void *v );
void *context_get( _context *ctx );

#endif
/* ************************************************************************ */
