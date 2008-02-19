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
#ifndef _NEKO_CONTEXT_H
#define _NEKO_CONTEXT_H

typedef struct _context _context;
typedef struct _clock _clock;

#ifdef NEKO_NO_THREADS

#define context_new() NULL
#define context_delete(ctx)
#define context_set(ctx,v)	ctx = ((_context*)(v))
#define context_get(ctx)	((void*)(ctx))

#define context_lock_new()	NULL
#define context_lock(l)
#define context_release(l)
#define context_lock_delete(l)

#else

_context *context_new();
void context_delete( _context *ctx );
void context_set( _context *ctx, void *v );
void *context_get( _context *ctx );

_clock *context_lock_new();
void context_lock( _clock *l );
void context_release( _clock *l );
void context_lock_delete( _clock *l );

#endif

#endif
/* ************************************************************************ */
