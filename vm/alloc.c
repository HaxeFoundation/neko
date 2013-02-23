/*
 * Copyright (C)2005-2012 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <string.h>
#include <stdio.h>
#include "neko.h"
#include "objtable.h"
#include "opcodes.h"
#include "vm.h"
#include "neko_mod.h"
#include "neko_vm.h"

#ifdef NEKO_POSIX
#	include <signal.h>
#endif

#ifdef NEKO_WINDOWS
#ifdef NEKO_STANDALONE
#	define GC_NOT_DLL
#else
#	define GC_DLL
#endif
#	define GC_WIN32_THREADS
#endif

#define GC_THREADS
#include "gc/gc.h"

#ifndef GC_MALLOC
#	error Looks like libgc was not installed, please install it before compiling
#else

// activate to get debug informations about the GC
// #define GC_LOG

#define gc_alloc			GC_MALLOC
#define gc_alloc_private	GC_MALLOC_ATOMIC
#define gc_alloc_big(n)			(((n) > 256) ? GC_MALLOC_IGNORE_OFF_PAGE(n) : GC_MALLOC(n))
#define gc_alloc_private_big(n)	(((n) > 256) ? GC_MALLOC_ATOMIC_IGNORE_OFF_PAGE(n) : GC_MALLOC_ATOMIC(n))
#define gc_alloc_root		GC_MALLOC_UNCOLLECTABLE
#define gc_free_root		GC_FREE

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
mt_lock *neko_fields_lock = NULL;
mt_local *neko_vm_context = NULL;
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

#if defined (GC_LOG) && defined(NEKO_POSIX) 
static void handle_signal( int signal ) {
	// reset to default handler
	struct sigaction act;
	act.sa_sigaction = NULL;
	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGSEGV,&act,NULL);
	// print signal VM stack
	printf("**** SIGNAL %d CAUGHT ****\n",signal);
	neko_vm_dump_stack(neko_vm_current());
	// signal again
	raise(signal);
}
#endif

static void null_warn_proc( char *msg, int arg ) {
#	ifdef GC_LOG
	printf(msg,arg);
	if( strstr(msg,"very large block") )
		neko_vm_dump_stack(neko_vm_current());
#	endif
}

void neko_gc_init() {
	GC_set_warn_proc((GC_warn_proc)(void*)null_warn_proc);
#	ifndef NEKO_WINDOWS
	// we can't set this on windows with old GC since
	// it's already initialized through its own DllMain
	GC_all_interior_pointers = 0;
#	endif
#if (GC_VERSION_MAJOR >= 7) && defined(NEKO_WINDOWS)
	GC_all_interior_pointers = 0;
#	ifndef NEKO_STANDALONE
	GC_use_DllMain(); // needed to auto-detect threads created by Apache
#	endif
#endif
	GC_java_finalization = 1;
	GC_init();
	GC_no_dls = 1;
#ifdef LOW_MEM
	GC_dont_expand = 1;
#endif
	GC_clear_roots();
#if defined(GC_LOG) && defined(NEKO_POSIX)
	{
		struct sigaction act;
		act.sa_sigaction = NULL;
		act.sa_handler = handle_signal;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		sigaction(SIGSEGV,&act,NULL);
	}
#endif
}

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

EXTERN char *alloc( unsigned int nbytes ) {
	return (char*)gc_alloc_big(nbytes);
}

EXTERN char *alloc_private( unsigned int nbytes ) {
	return (char*)gc_alloc_private_big(nbytes);
}

EXTERN value alloc_empty_string( unsigned int size ) {
	vstring *s;
	if( size == 0 )
		return (value)&empty_string;
	if( size > max_string_size )
		failure("max_string_size reached");
	s = (vstring*)gc_alloc_private_big(size+sizeof(vstring));
	s->t = VAL_STRING | (size << TAG_BITS);
	(&s->c)[size] = 0;
	return (value)s;
}

EXTERN value alloc_string( const char *str ) {
	if( str == NULL )
		return val_null;
	return copy_string(str,strlen(str));
}

EXTERN value alloc_float( tfloat f ) {
	vfloat *v = (vfloat*)gc_alloc_private(sizeof(vfloat));
	v->t = VAL_FLOAT;
	v->f = f;
	return (value)v;
}

EXTERN value alloc_int32( int i ) {
	vint32 *v = (vint32*)gc_alloc_private(sizeof(vint32));
	v->t = VAL_INT32;
	v->i = i;
	return (value)v;
}

EXTERN value alloc_array( unsigned int n ) {
	value v;
	if( n == 0 )
		return (value)(void*)&empty_array;
	if( n > max_array_size )
		failure("max_array_size reached");
	v = (value)gc_alloc_big(sizeof(varray)+(n - 1)*sizeof(value));
	v->t = VAL_ARRAY | (n << TAG_BITS);
	return v;
}

EXTERN value alloc_abstract( vkind k, void *data ) {
	vabstract *v = (vabstract*)gc_alloc(sizeof(vabstract));
	v->t = VAL_ABSTRACT;
	v->kind = k;
	v->data = data;
	return (value)v;
}

EXTERN value alloc_function( void *c_prim, unsigned int nargs, const char *name ) {
	vfunction *v;
	if( c_prim == NULL || (nargs < 0 && nargs != VAR_ARGS) )
		failure("alloc_function");
	v = (vfunction*)gc_alloc(sizeof(vfunction));
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
	v = (vfunction*)gc_alloc(sizeof(vfunction));
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
	vfunction *v = (vfunction*)gc_alloc(sizeof(vfunction));
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
	v = (vobject*)gc_alloc(sizeof(vobject));
	v->t = VAL_OBJECT;
	if( cpy == NULL || val_is_null(cpy) ) {
		v->proto = NULL;
		otable_init(&v->table);

	} else {
		v->proto = ((vobject*)cpy)->proto;
		otable_copy(&((vobject*)cpy)->table,&v->table);
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
	otable_replace(&((vobject*)obj)->table,f,v);
}

static void __on_finalize( value v, void *f ) {
	((finalizer)f)(v);
}

EXTERN void val_gc(value v, finalizer f ) {
	if( !val_is_abstract(v) )
		failure("val_gc");
	if( f )
		GC_REGISTER_FINALIZER_NO_ORDER(v,(GC_finalization_proc)__on_finalize,f,0,0);
	else
		GC_REGISTER_FINALIZER_NO_ORDER(v,NULL,NULL,0,0);
}

EXTERN value *alloc_root( unsigned int nvals ) {
	return (value*)gc_alloc_root(nvals*sizeof(value));
}

EXTERN void free_root(value *v) {
	gc_free_root(v);
}

extern void neko_init_builtins();
extern void neko_init_fields();
extern void neko_init_jit();
extern void neko_free_jit();

#define INIT_ID(x)	id_##x = val_id("__" #x)

EXTERN void neko_global_init() {
#	ifdef NEKO_DIRECT_THREADED
	op_last = neko_get_ttable()[Last];
#	endif
	empty_array.ptr = val_null;
	neko_gc_init();
	neko_vm_context = alloc_local();
	neko_fields_lock = alloc_lock();
	neko_fields = (objtable*)alloc_root((NEKO_FIELDS_MASK+1) * sizeof(struct _objtable) / sizeof(value));
	{
		int i;
		for(i=0;i<=NEKO_FIELDS_MASK;i++)
			otable_init(&neko_fields[i]);
	}
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
	free_local(neko_vm_context);
	free_lock(neko_fields_lock);
	neko_gc_major();
}

EXTERN void neko_set_stack_base( void *s ) {
	// deprecated
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
