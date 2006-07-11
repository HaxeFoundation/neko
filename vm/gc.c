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
#include "gc.h"
#ifdef NEKO_WINDOWS
#	include <windows.h>
#else
#	include <stdint.h>
#endif

#include <stddef.h>
#include <malloc.h>
#include <stdio.h>
#include <setjmp.h>

#define PAGE_BITS	20
#define PAGE_SIZE	(1 << PAGE_BITS)
#define PAGE_COUNT	(1 << (32 - PAGE_BITS))
#define GC_BITS		2
#define GC_MASK		((1 << GC_BITS) - 1)
#define MAGIC		(0xFF010203 << (GC_BITS + 1))
#define NO_SCAN		(1 << GC_BITS)
#define MAGIC_FREE	0x00ABCDEF

#define ASSERT()		*(char*)NULL = 0
#define DELTA(ptr,x)	((bheader*)(((char*)ptr) + x))

//#define USE_STATS

#ifdef USE_STATS
#define NSIZES	8
struct stats {
	int current_pages;
	int current_bytes;
	int current_blocks;	
	int current_finalizers;
	int current_roots;
	int total_requested_bytes;
	int total_blocks;
	int total_finalizers;
	int total_roots;
	int blocks_by_size[NSIZES];
} stats;
#	define STATS_SET(s,v)	stats.s = v
#	define STATS(s,dec)	stats.s += dec
#else
#	define STATS_SET(s,v)
#	define STATS(s,dec)
#endif

struct infos {
	int sweep_blocks;
	int mark_blocks;
	double living_ratio;
	int last_gc;
	int next_gc;
} infos;

typedef struct {
	unsigned int magic;
	unsigned int size;
} bheader;

typedef struct gc_page {
	bheader *ptr;
	bheader *base;
	bheader *end;
	struct gc_page *next;
} gc_page;

typedef struct gc_root {
	void *ptr;
	struct gc_root *next;
} gc_root;

typedef struct gc_final {
	bheader *blk;
	gc_final_fun callb;
	struct gc_final *next;
} gc_final;

static gc_page *pages = NULL;
static gc_page *full_pages = NULL;
static gc_root *roots = NULL;
static gc_final *finals = NULL;
static int prev_gc, cur_gc;
static int page_bits[PAGE_COUNT];
static int page_count = 0;
static char **base_stack;
#ifdef NEKO_WINDOWS
static CRITICAL_SECTION plock;
#endif

static void gc_mark();
static void gc_sweep();
static void gc_finalize();

void neko_gc_init( void *s ) {
	int i;
	for(i=0;i<PAGE_COUNT;i++)
		page_bits[i] = 0;
	base_stack = s;
	prev_gc = 0;
	cur_gc = MAGIC;
	memset(&infos,0,sizeof(infos));
#ifdef USE_STATS
	memset(&stats,0,sizeof(struct stats));
#endif
#ifdef NEKO_WINDOWS
	InitializeCriticalSection(&plock);
#endif
}

// multiple threads = multiple stacks !
void neko_gc_set_stack_base( void *s ) {
	base_stack = s;
}

static int DELTA[4] = { 0, 3, 2, 1 };

static void dump_full_stats() {
#ifdef USE_STATS
	int i;
	printf("pages = %d, mem_req = %d, mem_cur = %d / %d\n",
		stats.current_pages,
		stats.total_requested_bytes >> 10,
		stats.current_bytes >> 10,
		(stats.current_pages * PAGE_SIZE) >> 10
	);
	printf("blocks = %d / %d, roots = %d / %d, final = %d / %d\n",
		stats.current_blocks,
		stats.total_blocks,
		stats.current_roots,
		stats.total_roots,
		stats.current_finalizers,
		stats.total_finalizers
	);
	for(i=0;i<NSIZES;i++) {
		int n = stats.blocks_by_size[i];
		if( n > 0 ) 
			printf("blocks[%d] = %d (%.2f%%)\n",i,n,n * 100.0 / stats.total_blocks);
	}
#endif
}

static void dump_stats() {
#ifdef USE_STATS
	printf("pages = %d, mem = %d/%d, tot = %d, living = %d, gc = %d\n",
		stats.current_pages,
		stats.current_bytes >> 10,
		(stats.current_pages * PAGE_SIZE) >> 10,
		stats.total_blocks,
		infos.mark_blocks,
		infos.sweep_blocks
	);
#endif
}
static void *alloc_page( int npages ) {	
	while( 1 ) {
		void *addr =  (void*)(uintptr_t)(page_count * PAGE_SIZE);
		if( page_count >= PAGE_COUNT )
			return NULL;
		addr = VirtualAlloc(addr,npages * PAGE_SIZE,MEM_RESERVE,PAGE_READWRITE);
		if( addr == NULL || (((uintptr_t)addr) & ((1 << PAGE_BITS) - 1)) != 0 ) {
			if( addr != NULL )
				VirtualFree(addr,0,MEM_RELEASE);
			page_count++;
		} else {
			int pc ;
			addr = VirtualAlloc(addr,npages * PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE);
			pc = (int)(((uintptr_t)addr) >> PAGE_BITS);
			if( pc < page_count )
				ASSERT();
			page_count = pc;
			STATS(current_pages,npages);
			while( npages-- )
				page_bits[page_count++] = 1;
			return addr;
		}
	}
}

static void gc_lock( int flag ) {
#ifdef NEKO_WINDOWS
	if( flag )
		EnterCriticalSection(&plock);
	else
		LeaveCriticalSection(&plock);
#endif
}

void neko_gc_collect() {
	gc_lock(1);
	while( pages != NULL ) {
		gc_page *tmp = pages->next;
		pages->next = full_pages;
		full_pages = pages;
		pages = tmp;
	}
	gc_mark();
	gc_sweep();
	gc_finalize();
	if( infos.mark_blocks > 0 )
		infos.living_ratio = infos.living_ratio * 0.2 + (infos.sweep_blocks * 1.0 / (infos.sweep_blocks + infos.mark_blocks)) * 0.8;
	infos.next_gc = (int)(0.25 / infos.living_ratio) * (infos.last_gc ? infos.last_gc : 1);
	infos.next_gc = (infos.last_gc + infos.next_gc) / 2;
	infos.last_gc = infos.next_gc;
	dump_stats();
	gc_lock(0);
}

void neko_gc_close() {
	neko_gc_collect();
	dump_full_stats();
#ifdef NEKO_WINDOWS
	DeleteCriticalSection(&plock);
#endif
}

static bheader *gc_alloc_block( unsigned int start_size, int can_gc ) {
	gc_page *p = pages;
	int npages;
	unsigned int size = start_size + sizeof(bheader);
	size += DELTA[size&3];
	while( p != NULL ) {
		bheader *c = p->ptr;
		while( c != p->end ) {
			if( c->magic == MAGIC_FREE && c->size >= size )
				break;
			c = DELTA(c,c->size);
		}
		if( c != p->end ) {
			c->magic = cur_gc;
			if( c->size - size > sizeof(bheader) + 4 ) {
				p->ptr = DELTA(c,size);
				p->ptr->magic = MAGIC_FREE;
				p->ptr->size = c->size - size;
				c->size = size;
			} else {
				size = c->size;
				p->ptr = DELTA(c,size);
			}
#ifdef USE_STATS
			if( (start_size & 3) == 0 && (start_size >> 2) < NSIZES )
				stats.blocks_by_size[start_size>>2]++;
#endif
			STATS(total_blocks,1);
			STATS(total_requested_bytes,start_size);
			STATS(current_blocks,1);
			STATS(current_bytes,size);
			return c;
		}
		// remove page from page list
		pages = p->next;
		p->next = full_pages;
		full_pages = p;
		p = pages;
	}
	// run a gc phase ?
	if( can_gc && infos.next_gc-- == 0 ) {
		neko_gc_collect();
		return gc_alloc_block(start_size,0);
	}
	// alloc a new page
	p = malloc(sizeof(gc_page));
	if( p == NULL )
		ASSERT();
	npages = (size > PAGE_SIZE)?((size + PAGE_SIZE - 1)/PAGE_SIZE):1;
	p->base = alloc_page(npages);
	if( p->base == NULL )
		ASSERT();
	p->base->magic = MAGIC_FREE;
	p->base->size = npages * PAGE_SIZE;
	p->ptr = p->base;
	p->end = DELTA(p->base,npages * PAGE_SIZE);
	p->next = pages;
	pages = p;
	return gc_alloc_block(start_size,0);
}

void *neko_gc_alloc( unsigned int size ) {
	bheader *c;
	gc_lock(1);
	c = gc_alloc_block(size,1);
	c++;
	gc_lock(0);
	return c;
}

void *neko_gc_alloc_private( unsigned int size ) {
	bheader *c;
	gc_lock(1);
	c = gc_alloc_block(size,1);
	c->magic |= NO_SCAN;
	c++;
	gc_lock(0);
	return c;
}

static void gc_add_root( void *gc_block ) {
	gc_root *r = (gc_root*)malloc(sizeof(gc_root));
	r->ptr = gc_block;
	r->next = roots;
	roots = r;
}

void *neko_gc_alloc_root( unsigned int size ) {
	bheader *r = neko_gc_alloc(size);
	gc_lock(1);
	gc_add_root(r);
	STATS(current_roots,1);
	STATS(total_roots,1);
	gc_lock(0);
	return r;
}

void neko_gc_free_root( void *gc_block ) {
	gc_root *r;
	gc_root *prev = NULL;
	gc_lock(1);
	r = roots;
	while( r != NULL ) {
		if( r->ptr == gc_block ) {
			STATS(current_roots,-1);
			if( prev == NULL ) {
				roots = roots->next;
				gc_lock(0);
				free(r);
				return;
			} else {
				prev->next = r->next;
				gc_lock(0);
				free(r);
				return;
			}
		}
		prev = r;
		r = r->next;
	}
	gc_lock(0);
}

void neko_gc_finalizer( void *gc_block, gc_final_fun callb ) {
	gc_final *f, *prev = NULL;
	bheader *blk = (bheader*)gc_block - 1;
	gc_lock(1);
	f = finals;
	while( f != NULL ) {
		if( f->blk == blk ) {
			if( callb != NULL ) {
				f->callb = callb;
				return;
			}
			if( prev == NULL )
				finals = f->next;
			else
				prev->next = f->next;
			STATS(current_finalizers,-1);
			gc_lock(0);
			free(f);
			return;
		}
		f = f->next;
	}
	if( callb != NULL ) {
		f = (gc_final*)malloc(sizeof(gc_final));
		f->blk = blk;
		f->callb = callb;
		f->next = finals;
		finals = f;
		STATS(current_finalizers,1);
		STATS(total_finalizers,1);
	}
	gc_lock(0);
}

// --------- MARK & SWEEP --------------------------

static void gc_mark_block( void *_p ) {
	bheader *c = ((bheader*)_p) - 1;
	int i;
	if( ((uintptr_t)c) & 3 )
		return;
	i = (int)(((uintptr_t)c) >> PAGE_BITS);
	if( i >= page_count || !page_bits[i] )
		return;
	i = c->magic & ~NO_SCAN;
	if( i != prev_gc || i == cur_gc )
		return;
	i = c->magic & NO_SCAN;
	c->magic = cur_gc | i;
	infos.mark_blocks++;
	if( i )
		return;
	i = (c->size - sizeof(bheader)) / sizeof(void*);
	switch( i ) {
	case 5:
		gc_mark_block( ((void**)_p)[4] );
	case 4:
		gc_mark_block( ((void**)_p)[3] );
	case 3:
		gc_mark_block( ((void**)_p)[2] );
	case 2:
		gc_mark_block( ((void**)_p)[1] );
	case 1:
		gc_mark_block( ((void**)_p)[0] );
	case 0:
		break;
	default:
		while( i-- )
			gc_mark_block( ((void**)_p)[i] );
		break;

	}
}

static void gc_mark() {
	jmp_buf b;
	gc_root *r = roots;
	prev_gc = cur_gc;
	cur_gc = MAGIC | ((cur_gc + 1) & GC_MASK);
	infos.mark_blocks = 0;
	while( r != NULL ) {
		gc_mark_block(r->ptr);
		r = r->next;
	}
	if( setjmp(b) )
		return;	
	{
		char **pos = base_stack;
		while( pos > (char**)&pos ) {
			gc_mark_block(*pos);
			pos--;
		}
	}
	longjmp(b,1);
}

static void gc_sweep() {
	int nsweeps = 0;
	gc_page *p = full_pages, *tmp;
	full_pages = NULL;
	while( p != NULL ) {
		bheader *c = p->base;
		bheader *prev = NULL;
		while( c != p->end ) {
			if( (c->magic & ~NO_SCAN) != cur_gc ) {
				if( c->magic != MAGIC_FREE ) {
					c->magic = MAGIC_FREE;
					nsweeps++;
					STATS(current_bytes, -(int)c->size);
				}
				if( prev != NULL )
					prev->size += c->size;
				else
					prev = c;				
				c = DELTA(c,c->size);
			} else {
				prev = NULL;
				c = DELTA(c,c->size);
			}
		}
		tmp = p->next;
		p->ptr = p->base;
		p->next = pages;
		pages = p;
		p = tmp;
	}
	infos.sweep_blocks = nsweeps;
	STATS(current_blocks,-nsweeps);
}

static void gc_finalize() {
	gc_final *f = finals, *prev = NULL;
	while( f != NULL ) {
		if( (f->blk->magic & ~NO_SCAN) != cur_gc ) {
			if( prev == NULL )
				finals = f;
			else
				prev->next = f->next;
			// don't unlock in order to prevent GC calls 
			// from being done inside the finalizer
			// we don't want to have our block data erased...
			f->callb(f->blk + 1);
			STATS(current_finalizers,-1);
			free(f);
			if( prev == NULL)
				f = finals;
			else
				f = prev->next;
		} else
			f = f->next;
	}
}

/* ************************************************************************ */
