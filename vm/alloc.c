/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include "neko.h"
#include "objtable.h"
#include "load.h"
#include "vmcontext.h"
#ifdef _WIN32
#	define GC_DLL
#	define GC_THREADS
#	define GC_WIN32_THREADS
#endif
#include "gc/gc.h"

static val_type t_null = VAL_NULL;
static val_type t_true = VAL_BOOL;
static val_type t_false = VAL_BOOL;
EXTERN value val_null = (value)&t_null;
EXTERN value val_true = (value)&t_true;
EXTERN value val_false = (value)&t_false;

struct _otype {
	value (*init)();
};

static void null_warn_proc( char *msg, int arg ) {
}

void gc_init() {
	GC_no_dls = 1;
	GC_dont_expand = 1;
	GC_clear_roots();
	GC_set_warn_proc(null_warn_proc);
}

void gc_loop() {
	GC_collect_a_little();
}

void gc_major() {
	GC_gcollect();
}

EXTERN char *alloc( unsigned int nbytes ) {
	return GC_MALLOC(nbytes);
}

EXTERN char *alloc_abstract( unsigned int nbytes ) {
	return GC_MALLOC_ATOMIC(nbytes);
}

EXTERN value alloc_empty_string( unsigned int size ) {
	vstring *s;
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
	if( n > max_array_size )
		val_throw(alloc_string("max_array_size reached"));
	v = (value)GC_MALLOC(n*sizeof(value)+sizeof(val_type));
	v->t = VAL_ARRAY | (n << 3);
	return v;
}

EXTERN value alloc_function( void *c_prim, unsigned int nargs ) {
	vfunction *v;
	if( c_prim == NULL || (nargs < 0 && nargs != VAR_ARGS) )
		return val_null;
	v = (vfunction*)GC_MALLOC(sizeof(vfunction)-sizeof(void*));
	v->t = VAL_PRIMITIVE;
	v->addr = c_prim;
	v->nargs = nargs;
	v->env = alloc_array(0);
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

EXTERN value copy_object( value cpy ) {
	vobject *v;
	if( cpy != NULL && !val_is_null(cpy) && !val_is_object(cpy) )
		return val_null;
	v = (vobject*)GC_MALLOC(sizeof(vobject));
	v->t = VAL_OBJECT;
	if( cpy == NULL || val_is_null(cpy) ) {
		v->data = NULL;
		v->ot = (otype)0xFFFFFFFF;
		v->table = otable_empty();
	} else {
		v->data = ((vobject*)cpy)->data;
		v->ot =	((vobject*)cpy)->ot;
		v->table = otable_copy(((vobject*)cpy)->table);
	}
	return (value)v;
}

EXTERN value copy_string( const char *str, unsigned int strlen ) {
	value v = alloc_empty_string(strlen);
	char *c = (char*)val_string(v);
	memcpy(c,str,strlen);
	return v;
}

// not static, used by interp
value create_instance( value baseclass ) {
	value o;
	if( !val_is_object(baseclass) )
		return val_null;
	o = copy_object(baseclass);
	val_odata(o) = val_null;
	val_otype(o) = (otype)val_odata(baseclass);
	return o;
}

EXTERN value alloc_object( otype *t ) {
	value baseclass;
	if( !t )
		return copy_object(NULL);
	if( GC_base(*t) )
		baseclass = (value)(*t);
	else {
		baseclass = ((otype)t)->init();
 		*t = (otype)baseclass;
	}
	return create_instance(baseclass);
}

EXTERN value alloc_class( otype *t ) {
	value baseclass;
	if( !t ) {
		baseclass = copy_object(NULL);
		val_otype(baseclass) = t_class;
	} else if( GC_base(*t) )
		baseclass = (value)(*t);
	else {
		baseclass = (value)GC_MALLOC_UNCOLLECTABLE(sizeof(vobject));
		baseclass->t = VAL_OBJECT;
		val_otype(baseclass) = t_class;
		val_odata(baseclass) = baseclass;
		*t = (otype)baseclass;
	}
	return baseclass;
}

EXTERN void alloc_field( value obj, field f, value v ) {
	otable_replace(((vobject*)obj)->table,f,v);
}

static void __on_finalize(value v, void *f ) {
	((finalizer)f)(v);
}

EXTERN void val_gc(value v, finalizer f ) {
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

extern void init_builtins();
extern void free_builtins();

void neko_global_init() {
	gc_init();
	vm_context = context_new();
	init_builtins();
}

void neko_global_free() {
	free_builtins();
#ifdef _DEBUG
	if( roots != NULL ) {
		printf("Some roots are not free");
		*(char*)NULL = 0;
	}
	context_delete(roots_context);
#endif
	context_delete(vm_context);
	gc_major();
}

/* ************************************************************************ */
