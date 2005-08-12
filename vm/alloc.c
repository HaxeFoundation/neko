/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include "neko.h"
#include "objtable.h"
#include "load.h"
#include "opcodes.h"
#include "vmcontext.h"
#ifdef _WIN32
#	define GC_DLL
#	define GC_THREADS
#	define GC_WIN32_THREADS
#endif
#include "gc/gc.h"

static int op_last = Last;
int *callback_return = &op_last;
value *neko_builtins = NULL;
_context *neko_fields_context = NULL;
_context *neko_vm_context = NULL;
static val_type t_null = VAL_NULL;
static val_type t_true = VAL_BOOL;
static val_type t_false = VAL_BOOL;
static val_type t_array = VAL_ARRAY;
EXTERN value val_null = (value)&t_null;
EXTERN value val_true = (value)&t_true;
EXTERN value val_false = (value)&t_false;
static value empty_array = (value)&t_array;
static vstring empty_string = { VAL_STRING, 0 };
field id_compare;
field id_string;
field id_loader;
field id_exports;
field id_data;
field id_module;
field id_get, id_set;
field id_add, id_radd, id_sub, id_rsub, id_mult, id_rmult, id_div, id_rdiv, id_mod, id_rmod;

static void null_warn_proc( char *msg, int arg ) {
}

void neko_gc_init() {
	GC_no_dls = 1;
	GC_dont_expand = 1;
	GC_clear_roots();
	GC_set_warn_proc((GC_warn_proc)null_warn_proc);
}

EXTERN void neko_gc_loop() {
	GC_collect_a_little();
}

EXTERN void neko_gc_major() {
	GC_gcollect();
}

EXTERN char *alloc( unsigned int nbytes ) {
	return GC_MALLOC(nbytes);
}

EXTERN char *alloc_private( unsigned int nbytes ) {
	return GC_MALLOC_ATOMIC(nbytes);
}

EXTERN value alloc_empty_string( unsigned int size ) {
	vstring *s;
	if( size == 0 )
		return (value)&empty_string;
	if( size > max_string_size )
		val_throw(alloc_string("max_string_size reached"));
	s = (vstring*)GC_MALLOC_ATOMIC((size+1)+sizeof(val_type));
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
		return empty_array;
	if( n > max_array_size )
		val_throw(alloc_string("max_array_size reached"));
	v = (value)GC_MALLOC(n*sizeof(value)+sizeof(val_type));
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

EXTERN value alloc_function( void *c_prim, unsigned int nargs ) {
	vfunction *v;
	if( c_prim == NULL || (nargs < 0 && nargs != VAR_ARGS) )
		return val_null;
	v = (vfunction*)GC_MALLOC(sizeof(vfunction));
	v->t = VAL_PRIMITIVE;
	v->addr = c_prim;
	v->nargs = nargs;
	v->env = alloc_array(0);
	v->module = NULL;
	return (value)v;
}

value alloc_module_function( void *m, int pos, int nargs ) {
	vfunction *v;
	if( nargs < 0 && nargs != VAR_ARGS )
		return val_null;
	v = (vfunction*)GC_MALLOC(sizeof(vfunction));
	v->t = VAL_FUNCTION;
	v->addr = (void*)pos;
	v->nargs = nargs;
	v->env = alloc_array(0);
	v->module = m;
	return (value)v;
}

EXTERN value alloc_object( value cpy ) {
	vobject *v;
	if( cpy != NULL && !val_is_null(cpy) && !val_is_object(cpy) )
		return val_null;
	v = (vobject*)GC_MALLOC(sizeof(vobject));
	v->t = VAL_OBJECT;
	if( cpy == NULL || val_is_null(cpy) )
		v->table = otable_empty();
	else
		v->table = otable_copy(((vobject*)cpy)->table);
	return (value)v;
}

EXTERN value copy_string( const char *str, unsigned int strlen ) {
	value v = alloc_empty_string(strlen);
	char *c = (char*)val_string(v);
	memcpy(c,str,strlen);
	return v;
}

EXTERN void alloc_field( value obj, field f, value v ) {
	otable_replace(((vobject*)obj)->table,f,v);
}

static void __on_finalize(value v, void *f ) {
	((finalizer)f)(v);
}

EXTERN void val_gc(value v, finalizer f ) {
	if( !val_is_abstract(v) )
		return;
	if( f )
		GC_register_finalizer(v,(GC_finalization_proc)__on_finalize,f,0,0);
	else
		GC_register_finalizer(v,NULL,NULL,0,0);
}

#ifdef _DEBUG
#include <stdio.h>

typedef struct root_list {
	value *v;
	int size;
	int thread;
	struct root_list *next;
} root_list;
static _context *roots_context = NULL;
static root_list *roots = NULL;
static int thread_count = 0;
#endif

EXTERN value *alloc_root( unsigned int size ) {
	value *v = (value*)GC_MALLOC_UNCOLLECTABLE(size*sizeof(value));
#ifdef _DEBUG
	root_list *r = malloc(sizeof(root_list));
	if( roots_context == NULL )
		roots_context = context_new();
	if( context_get(roots_context) == NULL )
		context_set(roots_context,(void*)++thread_count);
	r->v = v;
	r->size = size;
	r->next = roots;
	r->thread = (int)context_get(roots_context);
	roots = r;
#endif
	return v;
}

EXTERN void free_root(value *v) {
#ifdef _DEBUG
	root_list *r = roots;
	root_list *prev = NULL;
	if( v == NULL )
		return;
	while( r != NULL && r->v != v ) {
		prev = r;
		r = r->next;
	}
	if( prev == NULL )
		roots = r->next;
	else
		prev->next = r->next;
	free(r);
#endif
	GC_free(v);
}

extern void neko_init_builtins();
extern void neko_init_fields();

#define INIT_ID(x)	id_##x = val_id("__" #x)

EXTERN void neko_global_init() {
	neko_gc_init();
	neko_vm_context = context_new();
	neko_fields_context = context_new();
	neko_init_builtins();
	id_loader = val_id("loader");
	id_exports = val_id("exports");
	INIT_ID(compare);
	INIT_ID(string);
	INIT_ID(data);
	INIT_ID(module);
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
}

EXTERN void neko_global_free() {
	val_clean_thread();
	free_root(neko_builtins);
#ifdef _DEBUG
	if( roots != NULL ) {
		printf("Some roots are not free");
		*(char*)NULL = 0;
	}
	context_delete(roots_context);
#endif
	context_delete(neko_vm_context);
	context_delete(neko_fields_context);
	neko_gc_major();
}

/* ************************************************************************ */
