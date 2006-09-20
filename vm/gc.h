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
#ifndef NEKO_GC_H
#define NEKO_GC_H

typedef void (*gc_final_fun)( void *blk );

void neko_gc_init( void *stack_base );
void neko_gc_set_stack_base( void *s );
void neko_gc_close();
void neko_gc_collect();
void *neko_gc_alloc( unsigned int size );
void *neko_gc_alloc_private( unsigned int size );
void *neko_gc_alloc_root( unsigned int size );
void neko_gc_free_root( void *gc_block );
void neko_gc_finalizer( void *gc_block, gc_final_fun f );
int neko_gc_heap_size();
int neko_gc_free_bytes();

#define GC_MALLOC neko_gc_alloc
#define GC_MALLOC_ATOMIC neko_gc_alloc_private
#define GC_MALLOC_UNCOLLECTABLE neko_gc_alloc_root
#define GC_collect_a_little neko_gc_collect
#define GC_gcollect neko_gc_collect
#define GC_free neko_gc_free_root
#define GC_get_heap_size neko_gc_heap_size
#define GC_get_free_bytes neko_gc_free_bytes

#endif

/* ************************************************************************ */
