/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "neko.h"
#include "load.h"
#include "objtable.h"
#include "vmcontext.h"

value *builtins = NULL;

#define BUILTIN(name,nargs)	builtins[p++] = alloc_function(builtin_##name,nargs)

static void builtin_error( const char *msg ) {
	val_throw(alloc_string(msg));
}

static value builtin_print( value *args, int nargs ) {
	buffer b;
	int i;
	if( nargs == 1 && val_is_string(*args) ) {
		val_print(*args);
		return val_true;
	}
	b = alloc_buffer(NULL);
	for(i=0;i<nargs;i++)
		val_buffer(b,args[i]);
	val_print(buffer_to_string(b));
	return val_true;
}

static value builtin_time() {
	return alloc_float( ((tfloat)clock()) / CLOCKS_PER_SEC );
}

static value builtin_new( value o ) {
	return copy_object(o);
}

static value builtin_array( value *args, int nargs ) {
	value a = alloc_array(nargs);
	int i;
	for(i=0;i<nargs;i++)
		val_array_ptr(a)[i] = args[i];
	return a;
}

static value builtin_amake( value size ) {
	value a;
	int i,s;
	if( !val_is_int(size) )
		return NULL;
	s = val_int(size);
	a = alloc_array(s);
	if( val_is_null(a) )
		return val_null;
	for(i=0;i<s;i++)
		val_array_ptr(a)[i] = val_null;
	return a;
}

static value builtin_acopy( value a ) {
	int i;
	value a2;
	if( !val_is_array(a) )
		return NULL;
	a2 = alloc_array(val_array_size(a));
	if( val_is_null(a) )
		return val_null;
	for(i=0;i<val_array_size(a);i++)
		val_array_ptr(a2)[i] = val_array_ptr(a)[i];
	return a2;
}

static value builtin_asize( value a ) {
	if( !val_is_array(a) )
		return NULL;
	return alloc_int( val_array_size(a) );
}

static value builtin_aget( value a, value p ) {
	int pp;
	if( !val_is_array(a) || !val_is_int(p) )
		builtin_error("aget");
	pp = val_int(p);
	if( pp < 0 || pp >= val_array_size(a) )
		builtin_error("aget");
	return val_array_ptr(a)[pp];
}

static value builtin_aset( value a, value p, value v ) {
	int pp;
	if( !val_is_array(a) || !val_is_int(p) )
		builtin_error("aset");
	pp = val_int(p);
	if( pp < 0 || pp >= val_array_size(a) )
		builtin_error("aset");
	val_array_ptr(a)[pp] = v;
	return v;
}

static value builtin_asub( value a, value p, value l ) {
	value a2;
	int i;
	int pp, ll;
	if( !val_is_array(a) || !val_is_int(p) || !val_is_int(l) )
		return val_null;
	pp = val_int(p);
	ll = val_int(l);
	if( pp < 0 || ll < 0 || pp+ll < 0 || pp+ll > val_array_size(a) )
		return val_null;
	a2 = alloc_array(ll);
	for(i=0;i<ll;i++)
		val_array_ptr(a2)[i] = val_array_ptr(a)[pp+i];
	return a2;
}

static value builtin_smake( value l ) {
	if( !val_is_int(l) )
		return val_null;
	return alloc_empty_string( val_int(l) );
}

static value builtin_ssize( value s ) {
	if( !val_is_string(s) )
		return val_null;
	return alloc_int(val_strlen(s));
}

static value builtin_scopy( value s ) {
	if( !val_is_string(s) )
		return val_null;
	return copy_string( val_string(s), val_strlen(s) );
}

static value builtin_ssub( value s, value p, value l ) {
	int pp , ll;
	if( !val_is_string(s) || !val_is_int(p) || !val_is_int(l) )
		return val_null;
	pp = val_int(p);
	ll = val_int(l);
	if( pp < 0 || ll < 0 || pp + ll < 0 || pp + ll > val_strlen(s) )
		return val_null;
	return copy_string( val_string(s) + pp , ll );
}

static value builtin_sget( value s, value p ) {
	int pp;
	if( !val_is_string(s) || !val_is_int(p) )
		return val_null;
	pp = val_int(p);
	if( pp < 0 || pp >= val_strlen(s) )
		return val_null;
	return alloc_int( val_string(s)[pp] );
}

static value builtin_sset( value s, value p, value c ) {
	int pp;
	unsigned char cc;
	if( !val_is_string(s) || !val_is_int(p) || !val_is_int(c) )
		return val_null;
	pp = val_int(p);
	if( pp < 0 || pp >= val_strlen(s) )
		return val_null;
	cc = (unsigned char)val_int(c);
	val_string(s)[pp] = (char)cc;
	return alloc_int(cc);
}

static value builtin_sblit( value dst, value dp, value src, value sp, value l ) {
	int dpp, spp, ll;
	if( !val_is_string(dst) || !val_is_int(dp) || !val_is_int(src) || !val_is_int(sp) || !val_is_int(l) )
		return val_null;
	dpp = val_int(dp);
	spp = val_int(sp);
	ll = val_int(l);
	if( dpp < 0 || spp < 0 || ll < 0 || dpp + ll < 0 || spp + ll  < 0 || dpp + ll > val_strlen(dst) || spp + ll > val_strlen(src) )
		return val_null;
	memcpy(val_string(dst)+dpp,val_string(src)+spp,ll);
	return val_true;
}

static value builtin_throw( value v ) {
	val_throw(v);
	return val_null;
}

static value builtin_isfun( value f, value nargs ) {
	int n;
	if( !val_is_function(f) || !val_is_int(nargs) )
		builtin_error("isfun");
	n = val_fun_nargs(f);
	if( n != val_int(nargs) && n != VAR_ARGS )
		builtin_error("isfun");
	return f;
}

static value builtin_nargs( value f ) {
	if( !val_is_function(f) )
		return val_null;
	return alloc_int( val_fun_nargs(f) );
}

static value builtin_callopt( value f, value args ) {
	int nargs;
	int asize;
	value *targs;
	if( !val_is_function(f) || val_is_array(args) )
		return val_null;
	nargs = val_fun_nargs(f);
	asize = val_array_size(args);
	if( nargs == VAR_ARGS )
		return val_callN(f,val_array_ptr(args),asize);
	if( nargs <= asize )
		return val_callN(f,val_array_ptr(args),nargs);
	targs = (value*)alloc(sizeof(value)*nargs);
	memcpy(targs,val_array_ptr(args),sizeof(value)*asize);
	while( asize < nargs )
		targs[asize++] = val_null;
	return val_callN(f,targs,nargs);
}

static value builtin_call( value f, value ctx, value args ) {
	value old;
	value ret;
	neko_vm *vm;
	if( !val_is_array(args) )
		return val_null;
	vm = NEKO_VM();
	old = vm->val_this;
	vm->val_this = ctx;
	ret = val_callN(f,val_array_ptr(args),val_array_size(args));
	vm->val_this = old;
	return ret;
}

static value builtin_div( value a, value b ) {
	if( !val_is_number(a) || !val_is_number(b) )
		return val_null;
	return alloc_float( ((tfloat)val_number(a)) / val_number(b) );
}

typedef union {
	double d;
	union {
		unsigned int l;
		unsigned int h;
	} i;
} qw;

static value builtin_isNaN( value f ) {
	qw q;
	unsigned int h, l;
	if( !val_is_float(f) )
		return val_false;
	q.d = val_float(f);
	h = q.i.h; 
	l = q.i.l;
	l = l | (h & 0xFFFFF);
	h = h & 0x7FF00000;
	return alloc_bool( h == 0x7FF00000 && l != 0 );
}

static value builtin_isInf( value f ) {
	qw q;
	unsigned int h, l;
	if( !val_is_float(f) )
		return val_false;
	q.d = val_float(f);
	h = q.i.h; 
	l = q.i.l;
	l = l | (h & 0xFFFFF);
	h = h & 0x7FF00000;
	return alloc_bool( h == 0x7FF00000 && l == 0 );
}

static value builtin_isTrue( value f ) {
	return alloc_bool(f != val_false && f != val_null && f != alloc_int(0));
}

static value builtin_objget( value o, value f ) {
	if( !val_is_string(f) )
		return val_null;
	return val_field(o,val_id(val_string(f)));
}

static value builtin_objset( value o, value f, value v ) {
	if( !val_is_string(f) )
		return val_null;
	alloc_field(o,val_id(val_string(f)),v);
	return v;
}

static value builtin_objcall( value o, value f, value args ) {
	if( !val_is_string(f) || !val_is_array(args) )
		return val_null;
	return val_ocallN(o,val_id(val_string(f)),val_array_ptr(args),val_array_size(args));
}

static value builtin_safeget( value o, value f ) {
	value *v;
	if( !val_is_string(f) || !val_is_object(o) )
		builtin_error("safeget");
	v = otable_find( ((vobject*)o)->table , val_id(val_string(f)) );
	if( v == NULL )
		builtin_error("safeget");
	return *v;
}

static value builtin_safeset( value o, value f, value r ) {
	value *v;
	if( !val_is_string(f) || !val_is_object(o) )
		builtin_error("safeset");
	v = otable_find( ((vobject*)o)->table , val_id(val_string(f)) );
	if( v == NULL )
		builtin_error("safeset");
	*v = r;
	return r;
}

static value builtin_safecall( value o, value f, value args ) {
	value *v, old;
	int fargs, nargs;
	neko_vm *vm;
	if( !val_is_string(f) || !val_is_object(o) || !val_is_array(args) )
		builtin_error("safecall");
	v = otable_find( ((vobject*)o)->table , val_id(val_string(f)) );
	if( v == NULL || !val_is_function(*v) )
		builtin_error("safecall");
	f = *v;
	fargs = val_fun_nargs(f);
	nargs = val_array_size(args);
	if( fargs != nargs && fargs != VAR_ARGS )
		builtin_error("safecall");
	vm = NEKO_VM();
	old = vm->val_this;
	vm->val_this = o;
	f = val_callN(f,val_array_ptr(args),nargs);
	vm->val_this = old;
	return f;
}

static value builtin_haveField( value o, value f ) {
	return alloc_bool( val_is_object(o) && val_is_string(f) && otable_find(((vobject*)o)->table, val_id(val_string(f))) != NULL );
}

static value builtin_objrem( value o, value f ) {
	if( !val_is_object(o) || !val_is_string(f) )
		return val_null;
	return alloc_bool( otable_remove(((vobject*)o)->table,val_id(val_string(f))) );
}

static value builtin_objopt( value o ) {
	if( !val_is_object(o) )
		return val_null;
	otable_optimize(((vobject*)o)->table);
	return val_true;
}

static void builtin_objFields_rec( value d, field id, void *a ) {
	*((*(value**)a)++) = alloc_int((int)id);
	
}

static value builtin_objFields( value o ) {
	value a;
	value *aptr;
	objtable t;
	if( !val_is_object(o) )
		return val_null;
	t = ((vobject*)o)->table;
	a = alloc_array(otable_count(t));
	aptr = val_array_ptr(a);
	otable_iter(t,builtin_objFields_rec,&aptr);
	return a;
}

static value builtin_setThis( value v ) {
	value old = NEKO_VM()->val_this;
	NEKO_VM()->val_this = v;
	return old;
}

static value builtin_hash( value f ) {
	if( !val_is_string(f) )
		return val_null;
	return alloc_int( (int)val_id(val_string(f)) );
}

static value builtin_field( value f ) {
	value *s;
	if( !val_is_int(f) )
		return val_null;
	s = otable_find(NEKO_VM()->fields,(field)val_int(f));
	if( s == NULL )
		return val_null;
	return *s;
}

static value builtin_int( value f ) {
	if( val_is_string(f) )
		return alloc_int( atoi(val_string(f)) );
	if( val_is_number(f) )
		return alloc_int( (int)val_number(f) );
	return val_null;
}

static value builtin_stof( value f ) {
	if( val_is_string(f) )
		return alloc_float( atof(val_string(f)) );
	return val_null;
}

static value builtin_typeof( value v ) {
	switch( val_type(v) ) {
	case VAL_INT:
		return alloc_int(1);
	case VAL_NULL:
		return alloc_int(0);
	case VAL_FLOAT:
		return alloc_int(2);
	case VAL_BOOL:
		return alloc_int(3);
	case VAL_STRING:
		return alloc_int(4);
	case VAL_OBJECT:
		return alloc_int(5);
	case VAL_ARRAY:
		return alloc_int(6);
	case VAL_FUNCTION:
	case VAL_PRIMITIVE:
		return alloc_int(7);
	default:
		return alloc_int(-1);
	}
}

static value closure_callback( value *args, int nargs ) {
	value *a;
	value env = NEKO_VM()->env;
	int cargs = val_array_size(env) - 1;
	value f = val_array_ptr(env)[0];
	int fargs = val_fun_nargs(f);
	int i;
	if( fargs != cargs + nargs && fargs != VAR_ARGS )
		return val_null;
	if( nargs == 0 )
		a = val_array_ptr(env) + 1;
	else if( cargs == 0 )
		a = args;
	else {
		a = (value*)alloc(sizeof(value)*(nargs+cargs));
		for(i=0;i<cargs;i++)
			a[i] = val_array_ptr(env)[i+1];
		for(i=0;i<nargs;i++)
			a[i+cargs] = args[i];
	}
	return val_callN(f,a,nargs+cargs);
}

static value builtin_closure( value *args, int nargs ) {
	value f;
	value env;
	int fargs;
	if( nargs == 0 )
		return val_null;
	f = args[0];
	if( !val_is_function(f) )
		return val_null;
	fargs = val_fun_nargs(f);
	if( fargs != VAR_ARGS && fargs < nargs-1 )
		return val_null;
	env = alloc_array(nargs);
	memcpy(val_array_ptr(env),args,nargs * sizeof(f));
	f = alloc_function( closure_callback, VAR_ARGS );
	((vfunction*)f)->env = env;
	return f;
}

static value builtin_compare( value a, value b ) {
	int r = val_compare(a,b);
	return (r == invalid_comparison)?val_null:alloc_int(r);
}

void init_builtins() {
	int p = 0;
	builtins = alloc_root(NBUILTINS);
	BUILTIN(print,VAR_ARGS);
	BUILTIN(time,0);
	BUILTIN(new,1);
	BUILTIN(array,VAR_ARGS);
	BUILTIN(amake,1);
	BUILTIN(acopy,1);
	BUILTIN(asize,1);
	BUILTIN(aget,2);
	BUILTIN(aset,3);
	BUILTIN(asub,3);
	BUILTIN(smake,1);
	BUILTIN(ssize,1);
	BUILTIN(scopy,1);
	BUILTIN(ssub,3);
	BUILTIN(sget,2);
	BUILTIN(sset,3);
	BUILTIN(sblit,5);
	BUILTIN(throw,1);
	BUILTIN(isfun,2);
	BUILTIN(nargs,1);
	BUILTIN(callopt,3);
	BUILTIN(call,3);
	BUILTIN(div,2);
	BUILTIN(isNaN,1);
	BUILTIN(isInf,1);
	BUILTIN(isTrue,1);
	BUILTIN(objget,2);
	BUILTIN(objset,3);
	BUILTIN(objcall,3);
	BUILTIN(safeget,2);
	BUILTIN(safeset,3);
	BUILTIN(safecall,3);
	BUILTIN(haveField,2);
	BUILTIN(objrem,2);
	BUILTIN(objopt,1);
	BUILTIN(objFields,1);
	BUILTIN(setThis,1);
	BUILTIN(hash,1);
	BUILTIN(field,1);
	BUILTIN(int,1);
	BUILTIN(stof,1);
	BUILTIN(typeof,1);
	BUILTIN(closure,VAR_ARGS);
	BUILTIN(compare,2);
	if( p != NBUILTINS )
		*(char*)NULL = 0;
}

void free_builtins() {
	free_root(builtins);
}

/* ************************************************************************ */
