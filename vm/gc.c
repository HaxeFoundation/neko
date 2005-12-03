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

#define PAGE_BITS	20
#define PAGE_SIZE	(1 << PAGE_BITS)
#define PAGE_COUNT	(1 << (32 - PAGE_BITS))
#define GC_BITS		2
#define GC_MASK		((1 << GC_BITS) - 1)
#define MAGIC		(0xFF010203 << GC_BITS)

#define ASSERT() *(char*)NULL = 0

typedef struct gc_page {
	char *ptr;
	unsigned int size;
	struct gc_page *next;
} gc_page;

typedef struct {
	int magic;
	int size;
} bheader;

typedef struct gc_root {
	void *ptr;
	struct gc_root *next;
} gc_root;

static gc_page *pages = NULL;
static gc_root *roots = NULL;
static int gc_flag = 0;
static int page_bits[PAGE_COUNT];
static int page_count = 0;
static int total_size = 0;
static int last_size = 0;
static char *base_stack;

static void gc_scan();
static void gc_sweep();

void gc_init() {
	int i;
	for(i=0;i<PAGE_COUNT;i++)
		page_bits[i] = 0;
	base_stack = (char*)&i;
}

static void gc_check() {
	gc_page *p = pages;
	int nblocks = 0;
	while( p != NULL ) {
		bheader *last = NULL;
		bheader *c = (bheader*)((p->ptr + p->size) - PAGE_SIZE);
		while( (char*)c != p->ptr ) {
			if( (c->magic & ~GC_MASK) != MAGIC ) {
				ASSERT();
				return;
			}
			last = c;
			c = (bheader*)(((char*)c) + c->size);
			nblocks++;
		}
		p = p->next;
	}
	printf("%d BLOCKS ALLOCATED\n",nblocks);
}

void gc_final() {
#ifdef _DEBUG
	gc_check();
#endif
}

static int DELTA[4] = { 0, 3, 2, 1 };

static void *alloc_page() {
	while( 1 ) {
		void *addr =  (void*)(uintptr_t)(page_count * PAGE_SIZE);
		addr = VirtualAlloc(addr,PAGE_SIZE,MEM_RESERVE,PAGE_READWRITE);
		if( addr == NULL || (((uintptr_t)addr) & ~(1 << PAGE_BITS)) != (uintptr_t)addr ) {
			if( addr != NULL )
				VirtualFree(addr,0,MEM_RELEASE);
			if( ++page_count == PAGE_COUNT )
				return NULL;
		} else {
			addr = VirtualAlloc(addr,PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE);
			page_count = (int)(((uintptr_t)addr) >> PAGE_BITS);
			//printf("ALLOC %d (%.8X - %.8X)\n",page_count,addr,(char*)addr+PAGE_SIZE);
			page_bits[page_count++] = 1;
			return addr;
		}
	}
}

void *gc_alloc( unsigned int size ) {
	gc_page *p = pages;
	size += sizeof(bheader);
	size += DELTA[size&3];
	while( p != NULL ) {
		if( p->size >= size ) {
			bheader *c = (bheader*)p->ptr;
			c->magic = MAGIC | gc_flag;
			c->size = size;
			p->size -= size;
			p->ptr += size;
			total_size += size;
			return c + 1;
		}
		p = p->next;
	}
	if( (last_size * 4) / 3 < total_size ) {
		gc_scan();
		gc_sweep();
		last_size = total_size;
	}
	if( size > PAGE_SIZE )
		ASSERT();
	p = malloc(sizeof(gc_page));
	if( p == NULL )
		ASSERT();
	p->ptr = alloc_page();
	if( p->ptr == NULL )
		ASSERT();
	p->size = PAGE_SIZE;
	p->next = pages;
	pages = p;
	return gc_alloc(size);
}

void *gc_alloc_private( unsigned int size ) {
	return gc_alloc(size);
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

// --------- SCAN --------------------------

static int scan_count;

static void gc_scan_block( void *_p ) {
	bheader *c = ((bheader*)_p) - 1;
	int i;
	if( ((uintptr_t)c) & 3 )
		return;
	i = (int)(((uintptr_t)c) >> PAGE_BITS);
	if( i >= page_count || !page_bits[i] )
		return;
	if( (c->magic & ~GC_MASK) != MAGIC )
		return;
	if( c->magic == (MAGIC | gc_flag) )
		return;
	c->magic = MAGIC | gc_flag;
	scan_count++;
	i = (c->size - sizeof(bheader)) / sizeof(void*);	
	while( i-- )
		gc_scan_block( ((void**)_p)[i] );
}

static void gc_scan() {
	char *pos;
	jmp_buf b;
	gc_root *r = roots;
	gc_flag++;
	gc_flag &= GC_MASK;
	scan_count = 0;
	while( r != NULL ) {
		gc_scan_block(r->ptr);
		r = r->next;
	}
	if( setjmp(b) ) {
		printf("SCAN %d BLOCKS (total %d bytes)\n",scan_count,total_size);
		return;
	}
	pos = base_stack;
	while( pos > (char*)&pos ) {
		gc_scan_block(*(char**)pos);
		pos -= 4;
	}
	longjmp(b,1);
}

static void gc_sweep() {
	
}
