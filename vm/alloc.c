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
#include <string.h>
#include "neko.h"
#include "objtable.h"
#include "opcodes.h"
#include "vm.h"
//#define NEKO_GC
#ifdef NEKO_GC
#	include "gc.h"
#else
#	ifdef NEKO_WINDOWS
#		ifdef NEKO_INSTALLER
#			define GC_NOT_DLL
#		else
#			define GC_DLL
#		endif
#		define GC_WIN32_THREADS
#	endif
#	define GC_THREADS
#	include "gc/gc.h"
#endif

#ifndef GC_MALLOC
#	error Looks like libgc was not installed, please install it before compiling
#else

#ifndef NEKO_WINDOWS
#	include <pthread.h>
#endif

typedef struct _klist {
	const char *name;
	vkind k;
	struct _klist *next;
} kind_list;

static int_val op_last = Last;
static value *apply_string = NULL;
int_val *callback_return = &op_last;
value *neko_builtins = NULL;
objtable *neko_fields = NULL;
_clock *neko_fields_lock = NULL;
_context *neko_vm_context = NULL;
static val_type t_null = VAL_NULL;
static val_type t_true = VAL_BOOL;
static val_type t_false = VAL_BOOL;
EXTERN value val_null = (value)&t_null;
EXTERN value val_true = (value)&t_true;
EXTERN value val_false = (value)&t_false;
static varray empty_array = { VAL_ARRAY, NULL };
static vstring empty_string = { VAL_STRING, 0 };
static kind_list **kind_names = NULL;
field id_compare;
field id_string;
field id_loader;
field id_exports;
field id_cache;
field id_path;
field id_loader_libs;
field id_get, id_set;
field id_add, id_radd, id_sub, id_rsub, id_mult, id_rmult, id_div, id_rdiv, id_mod, id_rmod;
EXTERN field neko_id_module;

#ifndef NEKO_GC

static void null_warn_proc( char *msg, int arg ) {
}

static void __on_finalize(value v, void *f ) {
	((finalizer)f)(v);
}

void neko_gc_init( void *ptr ) {
	GC_init();
	GC_no_dls = 1;
#ifdef LOW_MEM
	GC_dont_expand = 1;
#endif
	GC_clear_roots();
	GC_set_warn_proc((GC_warn_proc)(void*)null_warn_proc);
}

void neko_gc_close() {
}

void neko_gc_set_stack_base( void *ptr ) {
}

#endif

EXTERN void neko_gc_loop() {
	GC_collect_a_little();
}

EXTERN void neko_gc_major() {
	GC_gcollect();
}

EXTERN void neko_gc_stats( int *heap, int *free ) {
	*heap = (int)GC_get_heap_size();
	*free = (int)GC_get_free_bytes();
}

typedef struct {
	thread_main_func init;
	thread_main_func main;
	void *param;
#	ifdef NEKO_WINDOWS
	HANDLE lock;
#	else
	pthread_mutex_t lock;
#	endif
} tparams;

#ifdef NEKO_WINDOWS
static DWORD WINAPI ThreadMain( void *_p ) {
	tparams p = *(tparams*)_p;
	p.init(p.param);
	ReleaseSemaphore(p.lock,1,NULL);
	return p.main(p.param);
}
#else
static void *ThreadMain( void *_p ) {
	tparams *lp = (tparams*)_p;
	tparams p = *lp;
	p.init(p.param);
	// we have the 'param' value on this thread C stack
	// so it's safe to give back control to main thread
	pthread_mutex_unlock(&lp->lock);
	return (void*)(int_val)p.main(p.param);
}
#endif

EXTERN int neko_thread_create( thread_main_func init, thread_main_func main, void *param, void *handle ) {
	tparams p;
	p.init = init;
	p.main = main;
	p.param = param;
#	ifdef NEKO_WINDOWS
	{
		HANDLE h;
		p.lock = CreateSemaphore(NULL,0,1,NULL);
		h = GC_CreateThread(NULL,0,ThreadMain,&p,0,handle);
		if( h == NULL ) {
			CloseHandle(p.lock);
			return 0;
		}
		WaitForSingleObject(p.lock,INFINITE);
		CloseHandle(p.lock);
		CloseHandle(h);
		return 1;
	}
#	else
	pthread_mutex_init(&p.lock,NULL);
	pthread_mutex_lock(&p.lock);
	// force the use of a the GC method to capture created threads
	// this function should be defined in gc/gc.h
	if( GC_pthread_create((pthread_t*)handle,NULL,&ThreadMain,&p) != 0 ) {
		pthread_mutex_destroy(&p.lock);
		return 0;
	}
	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
	return 1;
#	endif
}


EXTERN char *alloc( unsigned int nbytes ) {
	return (char*)GC_MALLOC(nbytes);
}

EXTERN char *alloc_private( unsigned int nbytes ) {
	return (char*)GC_MALLOC_ATOMIC(nbytes);
}

EXTERN value alloc_empty_string( unsigned int size ) {
	vstring *s;
	if( size == 0 )
		return (value)&empty_string;
	if( size > max_string_size )
		failure("max_string_size reached");
	s = (vstring*)GC_MALLOC_ATOMIC(size+sizeof(vstring));
	s->t = VAL_STRING | (size << 3);
	(&s->c)[size] = 0;
	return (value)s;
}

EXTERN value alloc_string( const char *str ) {
	if( str == NULL )
		return val_null;
	return copy_string(str,strlen(str));
}

EXTERN value alloc_float( tfloat f ) {
	vfloat *v = (vfloat*)GC_MALLOC_ATOMIC(sizeof(vfloat));
	v->t = VAL_FLOAT;
	v->f = f;
	return (value)v;
}

EXTERN value alloc_array( unsigned int n ) {
	value v;
	if( n == 0 )
		return (value)(void*)&empty_array;
	if( n > max_array_size )
		failure("max_array_size reached");
	v = (value)GC_MALLOC(sizeof(varray)+(n - 1)*sizeof(value));
	v->t = VAL_ARRAY | (n << 3);
	return v;
}

EXTERN value alloc_abstract( vkind k, void *data ) {
	vabstract *v = (vabstract*)GC_MALLOC(sizeof(vabstract));
	v->t = VAL_ABSTRACT;
	v->kind = k;
	v->data = data;
	return (value)v;
}

EXTERN value alloc_function( void *c_prim, unsigned int nargs, const char *name ) {
	vfunction *v;
	if( c_prim == NULL || (nargs < 0 && nargs != VAR_ARGS) )
		failure("alloc_function");
	v = (vfunction*)GC_MALLOC(sizeof(vfunction));
	v->t = VAL_PRIMITIVE;
	v->addr = c_prim;
	v->nargs = nargs;
	v->env = alloc_array(0);
	v->module = alloc_string(name);
	return (value)v;
}

value neko_alloc_module_function( void *m, int_val pos, int nargs ) {
	vfunction *v;
	if( nargs < 0 && nargs != VAR_ARGS )
		failure("alloc_module_function");
	v = (vfunction*)GC_MALLOC(sizeof(vfunction));
	v->t = VAL_FUNCTION;
	v->addr = (void*)pos;
	v->nargs = nargs;
	v->env = alloc_array(0);
	v->module = m;
	return (value)v;
}

static value apply1( value p1 ) {
	value env = NEKO_VM()->env;
	value *a = val_array_ptr(env) + 1;
	int n = val_array_size(env) - 1;
	a[n-1] = p1;
	return val_callN(a[-1],a,n);
}

static value apply2( value p1, value p2 ) {
	value env = NEKO_VM()->env;
	value *a = val_array_ptr(env) + 1;
	int n = val_array_size(env) - 1;
	a[n-2] = p1;
	a[n-1] = p2;
	return val_callN(a[-1],a,n);
}

static value apply3( value p1, value p2, value p3 ) {
	value env = NEKO_VM()->env;
	value *a = val_array_ptr(env) + 1;
	int n = val_array_size(env) - 1;
	a[n-3] = p1;
	a[n-2] = p2;
	a[n-1] = p3;
	return val_callN(a[-1],a,n);
}

static value apply4( value p1, value p2, value p3, value p4 ) {
	value env = NEKO_VM()->env;
	value *a = val_array_ptr(env) + 1;
	int n = val_array_size(env) - 1;
	a[n-4] = p1;
	a[n-3] = p2;
	a[n-2] = p3;
	a[n-1] = p4;
	return val_callN(a[-1],a,n);
}

static value apply5( value p1, value p2, value p3, value p4, value p5 ) {
	value env = NEKO_VM()->env;
	value *a = val_array_ptr(env) + 1;
	int n = val_array_size(env) - 1;
	a[n-4] = p1;
	a[n-3] = p2;
	a[n-2] = p3;
	a[n-1] = p4;
	a[n-1] = p5;
	return val_callN(a[-1],a,n);
}

value neko_alloc_apply( int nargs, value env ) {
	vfunction *v = (vfunction*)GC_MALLOC(sizeof(vfunction));
	v->t = VAL_PRIMITIVE;
	switch( nargs ) {
	case 1: v->addr = apply1; break;
	case 2: v->addr = apply2; break;
	case 3: v->addr = apply3; break;
	case 4: v->addr = apply4; break;
	case 5: v->addr = apply5; break;
	default: failure("Too many apply arguments"); break;
	}
	v->nargs = nargs;
	v->env = env;
	v->module = *apply_string;
	return (value)v;
}

EXTERN value alloc_object( value cpy ) {
	vobject *v;
	if( cpy != NULL && !val_is_null(cpy) && !val_is_object(cpy) )
		val_throw(alloc_string("$new")); // 'new' opcode simulate $new
	v = (vobject*)GC_MALLOC(sizeof(vobject));
	v->t = VAL_OBJECT;
	if( cpy == NULL || val_is_null(cpy) ) {
		v->proto = NULL;
		v->table = otable_empty();
	} else {
		v->proto = ((vobject*)cpy)->proto;
		v->table = otable_copy(((vobject*)cpy)->table);
	}
	return (value)v;
}

EXTERN value copy_string( const char *str, int_val strlen ) {
	value v = alloc_empty_string((unsigned int)strlen);
	char *c = (char*)val_string(v);
	memcpy(c,str,strlen);
	return v;
}

EXTERN void alloc_field( value obj, field f, value v ) {
	otable_replace(((vobject*)obj)->table,f,v);
}

EXTERN void val_gc(value v, finalizer f ) {
	if( !val_is_abstract(v) )
		failure("val_gc");
#ifdef NEKO_GC
	neko_gc_finalizer(v,(gc_final_fun)f);
#else
	if( f )
		GC_register_finalizer(v,(GC_finalization_proc)__on_finalize,f,0,0);
	else
		GC_register_finalizer(v,NULL,NULL,0,0);
#endif
}

EXTERN value *alloc_root( unsigned int nvals ) {
	return (value*)GC_MALLOC_UNCOLLECTABLE(nvals*sizeof(value));
}

EXTERN void free_root(value *v) {
	GC_free(v);
}

extern void neko_init_builtins();
extern void neko_init_fields();
extern void neko_init_jit();
extern void neko_free_jit();

#define INIT_ID(x)	id_##x = val_id("__" #x)

EXTERN void neko_global_init( void *s ) {
#	ifdef NEKO_DIRECT_THREADED
	op_last = neko_get_ttable()[Last];
#	endif
	empty_array.ptr = val_null;
	neko_gc_init(s);
	neko_vm_context = context_new();
	neko_fields_lock = context_lock_new();
	neko_fields = (objtable*)alloc_root(1);
	*neko_fields = otable_empty();
	neko_init_builtins();
	kind_names = (kind_list**)alloc_root(1);
	*kind_names = NULL;
	id_loader = val_id("loader");
	id_exports = val_id("exports");
	id_cache = val_id("cache");
	id_path = val_id("path");
	id_loader_libs = val_id("__libs");
	neko_id_module = val_id("__module");
	INIT_ID(compare);
	INIT_ID(string);
	INIT_ID(add);
	INIT_ID(radd);
	INIT_ID(sub);
	INIT_ID(rsub);
	INIT_ID(mult);
	INIT_ID(rmult);
	INIT_ID(div);
	INIT_ID(rdiv);
	INIT_ID(mod);
	INIT_ID(rmod);
	INIT_ID(get);
	INIT_ID(set);
	apply_string = alloc_root(1);
	*apply_string = alloc_string("apply");
	neko_init_jit();
}

EXTERN void neko_global_free() {
	neko_free_jit();
	free_root((value*)kind_names);
	free_root(apply_string);
	free_root(neko_builtins);
	free_root((value*)neko_fields);
	apply_string = NULL;
	context_delete(neko_vm_context);
	context_lock_delete(neko_fields_lock);
	neko_gc_major();
	neko_gc_close();
}

EXTERN void neko_set_stack_base( void *s ) {
	neko_gc_set_stack_base(s);
}

EXTERN void kind_share( vkind *k, const char *name ) {
	kind_list *l = *kind_names;
	while( l != NULL ) {
		if( strcmp(l->name,name) == 0 ) {
			*k = l->k;
			return;
		}
		l = l->next;
	}
	l = (kind_list*)alloc(sizeof(kind_list));
	l->k = *k;
	l->name = name;
	l->next = *kind_names;
	*kind_names = l;
}

#endif

/* ************************************************************************ */
