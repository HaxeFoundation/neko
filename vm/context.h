/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#pragma once

typedef struct _context _context;

_context *context_new();
void context_delete( _context *ctx );
void context_set( _context *ctx, void *v );
void *context_get( _context *ctx );

/* ************************************************************************ */
