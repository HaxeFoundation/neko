#include "gc.h"
#ifndef _MSC_VER
#	include <stdint.h>
#else
#	include <windows.h>
#endif

#include <stddef.h>
#include <malloc.h>
#include <stdio.h>
#include <setjmp.h>

#define PAGE_BITS	16
#define PAGE_SIZE	(1 << PAGE_BITS)
#define PAGE_COUNT	(1 << (32 - PAGE_BITS))
#define GC_BITS		2
#define GC_MASK		((1 << GC_BITS) - 1)
#define MAGIC		(0xFF010203 << (GC_BITS + 1))
#define NO_SCAN		(1 << GC_BITS)
#define MAGIC_FREE	0x00ABCDEF

#define ASSERT()		*(char*)NULL = 0
#define DELTA(ptr,x)	((bheader*)(((char*)ptr) + x))

#define USE_STATS

#ifdef USE_STATS
struct stats {
	int npages;
	int real_npages;
	int sweep_blocks;
	int mark_blocks;
	int total_bytes;
	int alloc_bytes;
	int alloc_blocks;
	int current_bytes;
	int current_blocks;	
	double living_ratio;
	int last_gc;
	int next_gc;
} stats;
#	define STATS_SET(s,v)	stats.s = v
#	define STATS(s,dec)	stats.s += dec
#else
#	define STATS_SET(s,v)
#	define STATS(s,dec)
#endif

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

static gc_page *pages = NULL;
static gc_page *full_pages = NULL;
static gc_root *roots = NULL;
static int prev_gc, cur_gc;
static int page_bits[PAGE_COUNT];
static int page_count = 0;
static char **base_stack;

static void gc_mark();
static void gc_sweep();

void gc_init( void *s ) {
	int i;
	for(i=0;i<PAGE_COUNT;i++)
		page_bits[i] = 0;
	base_stack = s;
	prev_gc = 0xFFFFFFFF;
	cur_gc = MAGIC;
#ifdef USE_STATS
	memset(&stats,0,sizeof(struct stats));
#endif
}

static int DELTA[4] = { 0, 3, 2, 1 };

static void dump_stats() {
#ifdef USE_STATS
	printf("pages = %d, mem = %d/%d, tot = %d, living = %d, gc = %d\ngc_ratio = %f, ratio = %f, next = %d\n",
		stats.npages,
		stats.current_bytes >> 10,
		stats.total_bytes >> 10,
		stats.alloc_blocks,
		stats.mark_blocks,
		stats.sweep_blocks,
		stats.sweep_blocks * 1.0 / stats.mark_blocks,
		stats.living_ratio,
		stats.next_gc
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
			STATS(npages,1);
			STATS(real_npages,npages);
			STATS(total_bytes,npages * PAGE_SIZE);
			while( npages-- )
				page_bits[page_count++] = 1;
			return addr;
		}
	}
}

void gc_major() {
	gc_mark();
	gc_sweep();
	if( stats.current_blocks > 0 )
		stats.living_ratio = stats.living_ratio * 0.2 + (stats.sweep_blocks * 1.0 / (stats.sweep_blocks + stats.current_blocks)) * 0.8;
	stats.next_gc = (int)(0.25 / stats.living_ratio) * (stats.last_gc ? stats.last_gc : 1);
	stats.next_gc = (stats.last_gc + stats.next_gc) / 2;
	stats.last_gc = stats.next_gc;
	dump_stats();
}

bheader *gc_alloc_block( unsigned int start_size, int can_gc ) {
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
			int i;
			if( ((uintptr_t)c) & 3 )
				ASSERT();
			i = (int)(((uintptr_t)c) >> PAGE_BITS);
			if( i >= page_count || !page_bits[i] )
				ASSERT();
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
			STATS(alloc_blocks,1);
			STATS(alloc_bytes,start_size);
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
	if( can_gc && stats.next_gc-- == 0 ) {
		gc_major();
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

void *gc_alloc( unsigned int size ) {
	bheader *c = gc_alloc_block(size,1);
	return c + 1;
}

void *gc_alloc_private( unsigned int size ) {
	bheader *c = gc_alloc_block(size,1);
	//c->magic |= NO_SCAN;
	return c + 1;
}

void gc_add_root( void *gc_block ) {
	gc_root *r = (gc_root*)malloc(sizeof(gc_root));
	r->ptr = gc_block;
	r->next = roots;
	roots = r;	
}

void gc_free_root( void *gc_block ) {
	gc_root *r = roots;
	gc_root *prev = NULL;
	while( r != NULL ) {
		if( r->ptr == gc_block ) {
			if( prev == NULL ) {
				roots = roots->next;
				free(r);
				return;
			} else {
				prev->next = r->next;
				free(r);
				return;
			}
		}
		prev = r;
		r = r->next;
	}
}

void gc_finalizer( void *gc_block, gc_final_fun f ) {
}

// --------- MARK & SWEEP --------------------------

static int max_depth = 0;

static void gc_mark_block( void *_p, int depth ) {
	bheader *c = ((bheader*)_p) - 1;
	int i;
	if( ((uintptr_t)c) & 3 )
		return;
	i = (int)(((uintptr_t)c) >> PAGE_BITS);
	if( i >= page_count || !page_bits[i] )
		return;
	if( c->magic != prev_gc )
		return;
	if( c->magic == cur_gc )
		return;
	c->magic = cur_gc;
	STATS(mark_blocks,1);
	i = (c->size - sizeof(bheader)) / sizeof(void*);
	depth++;
	if( depth > max_depth * 3 / 2 )
		max_depth = depth;
	switch( i ) {
	case 5:
		gc_mark_block( ((void**)_p)[4], depth );
	case 4:
		gc_mark_block( ((void**)_p)[3], depth );
	case 3:
		gc_mark_block( ((void**)_p)[2], depth );
	case 2:
		gc_mark_block( ((void**)_p)[1], depth );
	case 1:
		gc_mark_block( ((void**)_p)[0], depth );
	case 0:
		break;
	default:
		while( i-- )
			gc_mark_block( ((void**)_p)[i], depth );
		break;

	}
}

static void gc_mark() {
	jmp_buf b;
	gc_root *r = roots;
	prev_gc = cur_gc;
	cur_gc = MAGIC | ((cur_gc + 1) & GC_MASK);
	STATS_SET(mark_blocks,0);
	while( r != NULL ) {
		gc_mark_block(r->ptr,0);
		r = r->next;
	}
	if( setjmp(b) )
		return;	
	{
		char **pos = base_stack;
		while( pos > (char**)&pos ) {
			gc_mark_block(*pos,0);
			pos--;
		}
	}
	longjmp(b,1);
}

static void gc_sweep() {
	gc_page *p = full_pages, *tmp;
	STATS_SET(sweep_blocks,0);
	full_pages = NULL;
	while( p != NULL ) {
		bheader *c = p->base;
		bheader *prev = NULL;
		while( c != p->end ) {
			if( c->magic != cur_gc ) {
#ifdef USE_STATS
				if( c->magic != MAGIC_FREE ) {
					stats.sweep_blocks++;
					stats.current_bytes -= c->size;
				}
#endif
				c->magic = MAGIC_FREE;
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
	STATS(current_blocks,-stats.sweep_blocks);
}
