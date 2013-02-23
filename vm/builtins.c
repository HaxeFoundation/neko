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
#include <stdlib.h>
#include <math.h>
#include "neko.h"
#include "objtable.h"
#include "vm.h"

#ifdef NEKO_MINGW
#	undef setjmp
#	define setjmp _setjmp
#endif

extern value *neko_builtins;

DEFINE_KIND(neko_k_kind);
DEFINE_KIND(k_old_int32);

/**
	<doc>
		<h1>Builtins</h1>
		<p>
			Builtins are basic operations that can be optimized by the Neko compiler.
		</p>
	</doc>
**/

/**	<doc><h2>Array Builtins</h2></doc> **/

/**
	$array : any* -> array
	<doc>Create an array from a list of values</doc>
**/
static value builtin_array( value *args, int nargs ) {
	value a = alloc_array(nargs);
	int i;
	for(i=0;i<nargs;i++)
		val_array_ptr(a)[i] = args[i];
	return a;
}

/**
	$amake : n:int -> array
	<doc>Create an array of size [n]</doc>
**/
static value builtin_amake( value size ) {
	value a;
	int i,s;
	val_check(size,int);
	s = val_int(size);
	a = alloc_array(s);
	for(i=0;i<s;i++)
		val_array_ptr(a)[i] = val_null;
	return a;
}

/**
	$acopy : array -> array
	<doc>Make a copy of an array</doc>
**/
static value builtin_acopy( value a ) {
	int i;
	value a2;
	val_check(a,array);
	a2 = alloc_array(val_array_size(a));
	for(i=0;i<val_array_size(a);i++)
		val_array_ptr(a2)[i] = val_array_ptr(a)[i];
	return a2;
}

/**
	$asize : array -> int
	<doc>Return the size of an array</doc>
**/
static value builtin_asize( value a ) {
	val_check(a,array);
	return alloc_int( val_array_size(a) );
}

/**
	$asub : array -> p:int -> l:int -> array
	<doc>
	Return [l] elements starting at position [p] of an array.
	An error occurs if out of array bounds.
	</doc>
**/
static value builtin_asub( value a, value p, value l ) {
	value a2;
	int i;
	int pp, ll;
	val_check(a,array);
	val_check(p,int);
	val_check(l,int);
	pp = val_int(p);
	ll = val_int(l);
	if( pp < 0 || ll < 0 || pp+ll < 0 || pp+ll > val_array_size(a) )
		neko_error();
	a2 = alloc_array(ll);
	for(i=0;i<ll;i++)
		val_array_ptr(a2)[i] = val_array_ptr(a)[pp+i];
	return a2;
}

/**
	$ablit : dst:array -> dst_pos:int -> src:array -> src_pos:int -> len:int -> void
	<doc>
	Copy [len] elements from [src_pos] of [src] to [dst_pos] of [dst].
	An error occurs if out of arrays bounds.
	</doc>
**/
static value builtin_ablit( value dst, value dp, value src, value sp, value l ) {
	int dpp, spp, ll;
	val_check(dst,array);
	val_check(dp,int);
	val_check(src,array);
	val_check(sp,int);
	val_check(l,int);
	dpp = val_int(dp);
	spp = val_int(sp);
	ll = val_int(l);
	if( dpp < 0 || spp < 0 || ll < 0 || dpp + ll < 0 || spp + ll  < 0 || dpp + ll > val_array_size(dst) || spp + ll > val_array_size(src) )
		neko_error();
	memmove(val_array_ptr(dst)+dpp,val_array_ptr(src)+spp,ll * sizeof(value));
	return val_null;
}

/**
	$aconcat : array array -> array
	<doc>
	Build a single array from several ones.
	</doc>
**/
static value builtin_aconcat( value arrs ) {
	int tot = 0;
	int len;
	int i;
	value all;
	val_check(arrs,array);
	len = val_array_size(arrs);
	for(i=0;i<len;i++) {
		value a = val_array_ptr(arrs)[i];
		val_check(a,array);
		tot += val_array_size(a);
	}
	all = alloc_array(tot);
	tot = 0;
	for(i=0;i<len;i++) {
		value a = val_array_ptr(arrs)[i];
		int j, max = val_array_size(a);
		for(j=0;j<max;j++)
			val_array_ptr(all)[tot++] = val_array_ptr(a)[j];
	}
	return all;
}

/**	<doc><h2>String Builtins</h2></doc> **/

/**
	$string : any -> string
	<doc>Convert any value to a string. This will make a copy of string.</doc>
**/
static value builtin_string( value v ) {
	buffer b = alloc_buffer(NULL);
	val_buffer(b,v);
	return buffer_to_string(b);
}

/**
	$smake : n:int -> string
	<doc>Return an uninitialized string of size [n]</doc>
**/
static value builtin_smake( value l ) {
	value v;
	val_check(l,int);
	v = alloc_empty_string( val_int(l) );
	memset(val_string(v),0,val_int(l));
	return v;
}

/**
	$ssize : string -> int
	<doc>Return the size of a string</doc>
**/
static value builtin_ssize( value s ) {
	val_check(s,string);
	return alloc_int(val_strlen(s));
}

/**
	$scopy : string -> string
	<doc>Make a copy of a string</doc>
**/
static value builtin_scopy( value s ) {
	val_check(s,string);
	return copy_string( val_string(s), val_strlen(s) );
}

/**
	$ssub : string -> p:int -> l:int -> string
	<doc>
	Return [l] chars starting at position [p] of a string.
	An error occurs if out of string bounds.
	</doc>
**/
static value builtin_ssub( value s, value p, value l ) {
	int pp , ll;
	val_check(s,string);
	val_check(p,int);
	val_check(l,int);
	pp = val_int(p);
	ll = val_int(l);
	if( pp < 0 || ll < 0 || pp + ll < 0 || pp + ll > val_strlen(s) )
		neko_error();
	return copy_string( val_string(s) + pp , ll );
}

/**
	$sget : string -> n:int -> int?
	<doc>Return the [n]th char of a string or [null] if out of bounds</doc>
**/
static value builtin_sget( value s, value p ) {
	int pp;
	val_check(s,string);
	val_check(p,int);
	pp = val_int(p);
	if( pp < 0 || pp >= val_strlen(s) )
		return val_null;
	return alloc_int( (unsigned char)(val_string(s)[pp]) );
}

/**
	$sset : string -> n:int -> c:int -> int?
	<doc>
	Set the [n]th char of a string to ([c] & 255).
	Returns the char set or [null] if out of bounds.
	</doc>
**/
static value builtin_sset( value s, value p, value c ) {
	int pp;
	unsigned char cc;
	val_check(s,string);
	val_check(p,int);
	val_check(c,int);
	pp = val_int(p);
	if( pp < 0 || pp >= val_strlen(s) )
		return val_null;
	cc = (unsigned char)val_int(c);
	val_string(s)[pp] = (char)cc;
	return alloc_int(cc);
}

/**
	$sblit : dst:string -> dst_pos:int -> src:string -> src_pos:int -> len:int -> void
	<doc>
	Copy [len] chars from [src_pos] of [src] to [dst_pos] of [dst].
	An error occurs if out of strings bounds.
	</doc>
**/
static value builtin_sblit( value dst, value dp, value src, value sp, value l ) {
	int dpp, spp, ll;
	val_check(dst,string);
	val_check(dp,int);
	val_check(src,string);
	val_check(sp,int);
	val_check(l,int);
	dpp = val_int(dp);
	spp = val_int(sp);
	ll = val_int(l);
	if( dpp < 0 || spp < 0 || ll < 0 || dpp + ll < 0 || spp + ll  < 0 || dpp + ll > val_strlen(dst) || spp + ll > val_strlen(src) )
		neko_error();
	memmove(val_string(dst)+dpp,val_string(src)+spp,ll);
	return val_null;
}

/**
	$sfind : src:string -> pos:int -> pat:string -> int?
	<doc>
	Return the first position starting at [pos] in [src] where [pat] was found.
	Return null if not found. Error if [pos] is outside [src] bounds.
	</doc>
**/
static value builtin_sfind( value src, value pos, value pat ) {
	int p, l, l2;
	const char *ptr;
	val_check(src,string);
	val_check(pos,int);
	val_check(pat,string);
	p = val_int(pos);
	l = val_strlen(src);
	l2 = val_strlen(pat);
	if( p < 0 || p >= l )
		neko_error();
	ptr = val_string(src) + p;
	while( l - p >= l2 ) {
		if( memcmp(ptr,val_string(pat),l2) == 0 )
			return alloc_int(p);
		p++;
		ptr++;
	}
	return val_null;
}

/** <doc><h2>Object Builtins</h2></doc> **/

/**
	$new : object? -> object
	<doc>Return a copy of the object or a new object if [null]</doc>
**/
static value builtin_new( value o ) {
	if( !val_is_null(o) && !val_is_object(o) )
		neko_error();
	return alloc_object(o);
}

/**
	$objget : o:any -> f:int -> any
	<doc>Return the field [f] of [o] or [null] if doesn't exists or [o] is not an object</doc>
**/
static value builtin_objget( value o, value f ) {
	if( !val_is_object(o) )
		return val_null; // keep dot-access semantics
	val_check(f,int);
	return val_field(o,val_int(f));
}

/**
	$objset : o:any -> f:int -> v:any -> any
	<doc>Set the field [f] of [o] to [v] and return [v] if [o] is an object or [null] if not</doc>
**/
static value builtin_objset( value o, value f, value v ) {
	if( !val_is_object(o) )
		return val_null; // keep dot-access semantics
	val_check(f,int);
	alloc_field(o,val_int(f),v);
	return v;
}

/**
	$objcall : o:any -> f:int -> args:array -> any
	<doc>Call the field [f] of [o] with [args] and return the value or [null] is [o] is not an object</doc>
**/
static value builtin_objcall( value o, value f, value args ) {
	if( !val_is_object(o) )
		return val_null; // keep dot-access semantics
	val_check(f,int);
	val_check(args,array);
	return val_ocallN(o,val_int(f),val_array_ptr(args),val_array_size(args));
}

/**
	$objfield : o:any -> f:int -> bool
	<doc>Return true if [o] is an object which have field [f]</doc>
**/
static value builtin_objfield( value o, value f ) {
	val_check(f,int);
	return alloc_bool( val_is_object(o) && otable_find(&((vobject*)o)->table, val_int(f)) != NULL );
}

/**
	$objremove : o:object -> f:int -> bool
	<doc>Remove the field [f] from object [o]. Return [true] on success</doc>
**/
static value builtin_objremove( value o, value f ) {
	val_check(o,object);
	val_check(f,int);
	return alloc_bool( otable_remove(&((vobject*)o)->table,val_int(f)) );
}

static void builtin_objfields_rec( value d, field id, void *a ) {
	*((*(value**)a)++) = alloc_int((int)id);
}

/**
	$objfields : o:object -> int array
	<doc>Return all fields of the object</doc>
**/
static value builtin_objfields( value o ) {
	value a;
	value *aptr;
	objtable *t;
	val_check(o,object);
	t = &((vobject*)o)->table;
	a = alloc_array(otable_count(t));
	aptr = val_array_ptr(a);
	otable_iter(t,builtin_objfields_rec,&aptr);
	return a;
}

/**
	$hash : string -> int
	<doc>Return the hashed value of a field name</doc>
**/
static value builtin_hash( value f ) {
	val_check(f,string);
	return alloc_int( (int)val_id(val_string(f)) );
}

/**
	$fasthash : string -> int
	<doc>Return the hashed value of a field name, without accessing the cache</doc>
**/
static value builtin_fasthash( value f ) {
	value acc = alloc_int(0);
	unsigned char *name;
	val_check(f,string);
	name = (unsigned char *)val_string(f);
	while( *name ) {
		acc = alloc_int(223 * val_int(acc) + *name);
		name++;
	}
	return acc;
}


/**
	$field : int -> string
	<doc>Reverse the hashed value of a field name. Return [null] on failure</doc>
**/
static value builtin_field( value f ) {
	val_check(f,int);
	return val_field_name(val_int(f));
}

/**
	$objsetproto : o:object -> proto:object? -> void
	<doc>Set the prototype of the object</doc>
**/
static value builtin_objsetproto( value o, value p ) {
	val_check(o,object);
	if( val_is_null(p) )
		((vobject*)o)->proto = NULL;
	else {
		val_check(p,object);
		((vobject*)o)->proto = (vobject*)p;
	}
	return val_null;
}

/**
	$objgetproto : o:object -> object?
	<doc>Get the prototype of the object</doc>
**/
static value builtin_objgetproto( value o ) {
	val_check(o,object);
	o = (value)((vobject*)o)->proto;
	if( o == NULL )
		return val_null;
	return o;
}

/** <doc><h2>Function Builtins</h2></doc> **/

/**
	$nargs : function -> int
	<doc>
	Return the number of arguments of a function.
	If the function have a variable number of arguments, it returns -1
	</doc>
**/
static value builtin_nargs( value f ) {
	val_check(f,function);
	return alloc_int( val_fun_nargs(f) );
}

/**
	$call : f:function -> this:any -> args:array -> any
	<doc>Call [f] with [this] context and [args] arguments</doc>
**/
static value builtin_call( value f, value ctx, value args ) {
	value old;
	value ret;
	neko_vm *vm;
	val_check(args,array);
	vm = NEKO_VM();
	old = vm->vthis;
	vm->vthis = ctx;
	ret = val_callN(f,val_array_ptr(args),val_array_size(args));
	vm->vthis = old;
	return ret;
}

static value closure_callback( value *args, int nargs ) {
	value env = NEKO_VM()->env;
	int cargs = val_array_size(env) - 2;
	value *a = val_array_ptr(env);
	value f = a[0];
	value o = a[1];
	int fargs = val_fun_nargs(f);
	int i;
	if( fargs != cargs + nargs && fargs != VAR_ARGS )
		return val_null;
	if( nargs == 0 )
		a = val_array_ptr(env) + 2;
	else if( cargs == 0 )
		a = args;
	else {
		a = (value*)alloc(sizeof(value)*(nargs+cargs));
		for(i=0;i<cargs;i++)
			a[i] = val_array_ptr(env)[i+2];
		for(i=0;i<nargs;i++)
			a[i+cargs] = args[i];
	}
	return val_callEx(o,f,a,nargs+cargs,NULL);
}

/**
	$closure : function -> object -> any* -> function
	<doc>Build a closure by applying a given number of arguments to a function</doc>
**/
static value builtin_closure( value *args, int nargs ) {
	value f;
	value env;
	int fargs;
	if( nargs <= 1 )
		failure("Invalid closure arguments number");
	f = args[0];
	if( !val_is_function(f) )
		neko_error();
	fargs = val_fun_nargs(f);
	if( fargs != VAR_ARGS && fargs < nargs-2 )
		failure("Invalid closure arguments number");
	env = alloc_array(nargs);
	memcpy(val_array_ptr(env),args,nargs * sizeof(f));
	f = alloc_function( closure_callback, VAR_ARGS, "closure_callback" );
	((vfunction*)f)->env = env;
	return f;
}

/**
	$apply : function -> any* -> any
	<doc>
	Apply the function to several arguments.
	Return a function asking for more arguments or the function result if more args needed.
	</doc>
**/
static value builtin_apply( value *args, int nargs ) {
	value f, env;
	int fargs;
	int i;
	nargs--;
	args++;
	if( nargs < 0 )
		neko_error();
	f = args[-1];
	if( !val_is_function(f) )
		neko_error();
	if( nargs == 0 )
		return f;
	fargs = val_fun_nargs(f);
	if( fargs == nargs || fargs == VAR_ARGS )
		return val_callN(f,args,nargs);
	if( nargs > fargs )
		neko_error();
	env = alloc_array(fargs + 1);
	val_array_ptr(env)[0] = f;
	for(i=0;i<nargs;i++)
		val_array_ptr(env)[i+1] = args[i];
	while( i++ < fargs )
		val_array_ptr(env)[i] = val_null;
	return neko_alloc_apply(fargs-nargs,env);
}

static value varargs_callback( value *args, int nargs ) {
	value f = NEKO_VM()->env;
	value a = alloc_array(nargs);
	int i;
	for(i=0;i<nargs;i++)
		val_array_ptr(a)[i] = args[i];
	return val_call1(f,a);
}

/**
	$varargs : f:function:1 -> function
	<doc>
	Return a variable argument function that, when called, will callback
	[f] with the array of arguments.
	</doc>
**/
static value builtin_varargs( value f ) {
	value fvar;
	val_check_function(f,1);
	fvar = alloc_function(varargs_callback,VAR_ARGS,"varargs");
	((vfunction*)fvar)->env = f;
	return fvar;
}

/** <doc><h2>Number Builtins</h2></doc> **/

/**
	$iadd : any -> any -> int
	<doc>Add two integers</doc>
**/
static value builtin_iadd( value a, value b ) {
	return alloc_best_int( val_any_int(a) + val_any_int(b) );
}

/**
	$isub : any -> any -> int
	<doc>Subtract two integers</doc>
**/
static value builtin_isub( value a, value b ) {
	return alloc_best_int( val_any_int(a) - val_any_int(b) );
}

/**
	$imult : any -> any -> int
	<doc>Multiply two integers</doc>
**/
static value builtin_imult( value a, value b ) {
	return alloc_best_int( val_any_int(a) * val_any_int(b) );
}

/**
	$idiv : any -> any -> int
	<doc>Divide two integers. An error occurs if division by 0</doc>
**/
static value builtin_idiv( value a, value b ) {
	if( val_any_int(b) == 0 )
		neko_error();
	return alloc_best_int( val_any_int(a) / val_any_int(b) );
}

typedef union {
	double d;
	struct {
		unsigned int l;
		unsigned int h;
	} i;
} qw;

/**
	$isnan : any -> bool
	<doc>Return if a value is the float NaN</doc>
**/
static value builtin_isnan( value f ) {
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

/**
	$isinfinite : any -> bool
	<doc>Return if a value is the float +Infinite</doc>
**/
static value builtin_isinfinite( value f ) {
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

/**
	$int : any -> int?
	<doc>Convert the value to the corresponding integer or return [null]</doc>
**/
static value builtin_int( value f ) {
	switch( val_type(f) ) {
	case VAL_FLOAT:
#ifdef	NEKO_WINDOWS
		return alloc_best_int((int)val_float(f));
#else
		// in case of overflow, the result is unspecified by ISO
		// so we have to make a module 2^32 before casting to int
		return alloc_int((unsigned int)fmod(val_float(f),4294967296.0));
#endif
	case VAL_STRING: {
		char *c = val_string(f), *end;
		int h;
		if( val_strlen(f) >= 2 && c[0] == '0' && (c[1] == 'x' || c[1] == 'X') ) {
			h = 0;
			c += 2;
			while( *c ) {
				char k = *c++;
				if( k >= '0' && k <= '9' )
					h = (h << 4) | (k - '0');
				else if( k >= 'A' && k <= 'F' )
					h = (h << 4) | ((k - 'A') + 10);
				else if( k >= 'a' && k <= 'f' )
					h = (h << 4) | ((k - 'a') + 10);
				else
					return val_null;
			}
			return alloc_best_int(h);
		}
		h = strtol(c,&end,10);
		return ( c == end ) ? val_null : alloc_best_int(h);
		}
	case VAL_INT:
	case VAL_INT32:
		return f;
	}
	return val_null;
}

/**
	$float : any -> float?
	<doc>Convert the value to the corresponding float or return [null]</doc>
**/
static value builtin_float( value f ) {
	if( val_is_string(f) ) {
		char *c = val_string(f), *end;
		tfloat f = (tfloat)strtod(c,&end);
		return (c == end) ? val_null : alloc_float(f);
	}
	if( val_is_number(f) )
		return alloc_float( val_number(f) );
	return val_null;
}

/** <doc><h2>Abstract Builtins</h2></doc> **/

/**
	$getkind : 'abstract -> 'kind
	<doc>Returns the kind value of the abstract</doc>
**/
static value builtin_getkind( value v ) {
	if( val_is_int32(v) )
		return alloc_abstract(neko_k_kind,k_old_int32);
	val_check(v,abstract);
	return alloc_abstract(neko_k_kind,val_kind(v));
}

/**
	$iskind : any -> 'kind -> bool
	<doc>Tells if a value is of the given kind</doc>
**/
static value builtin_iskind( value v, value k ) {
	val_check_kind(k,neko_k_kind);
	return val_is_abstract(v) ? alloc_bool(val_kind(v) == (vkind)val_data(k)) : (val_data(k) == k_old_int32 ? alloc_bool(val_is_int32(v)) : val_false);
}

/** <doc><h2>Hashtable Builtins</h2></doc> **/

/**
	$hkey : any -> int
	<doc>Return the hash of any value</doc>
**/
static value builtin_hkey( value v ) {
	return alloc_int(val_hash(v));
}

#define HASH_DEF_SIZE 7

/**
	$hnew : s:int -> 'hash
	<doc>Create an hashtable with [s] slots</doc>
**/
static value builtin_hnew( value size ) {
	vhash *h;
	int i;
	val_check(size,int);
	h = (vhash*)alloc(sizeof(vhash));
	h->nitems = 0;
	h->ncells = val_int(size);
	if( h->ncells <= 0 )
		h->ncells = HASH_DEF_SIZE;
	h->cells = (hcell**)alloc(sizeof(hcell*)*h->ncells);
	for(i=0;i<h->ncells;i++)
		h->cells[i] = NULL;
	return alloc_abstract(k_hash,h);
}

static void add_rec( hcell **cc, int size, hcell *c ) {
	int k;
	if( c == NULL )
		return;
	add_rec(cc,size,c->next);
	k = c->hkey % size;
	c->next = cc[k];
	cc[k] = c;
}

/**
	$hresize : 'hash -> int -> void
	<doc>Resize an hashtable</doc>
**/
static value builtin_hresize( value vh, value size ) {
	vhash *h;
	hcell **cc;
	int nsize;
	int i;
	val_check_kind(vh,k_hash);
	val_check(size,int);
	h = val_hdata(vh);
	nsize = val_int(size);
	if( nsize <= 0 )
		nsize = HASH_DEF_SIZE;
	cc = (hcell**)alloc(sizeof(hcell*)*nsize);
	memset(cc,0,sizeof(hcell*)*nsize);
	for(i=0;i<h->ncells;i++)
		add_rec(cc,nsize,h->cells[i]);
	h->cells = cc;
	h->ncells = nsize;
	return val_null;
}

/**
	$hget : 'hash -> k:any -> cmp:function:2? -> any
	<doc>
		Look for the value bound to the key [k] in the hashtable.
		Use the comparison function [cmp] or [$compare] if [null].
		Return [null] if no value is found.
	</doc>
**/
static value builtin_hget( value vh, value key, value cmp ) {
	vhash *h;
	hcell *c;
	if( !val_is_null(cmp) )
		val_check_function(cmp,2);
	val_check_kind(vh,k_hash);
	h = val_hdata(vh);
	c = h->cells[val_hash(key) % h->ncells];
	if( val_is_null(cmp) ) {
		while( c != NULL ) {
			if( val_compare(key,c->key) == 0 )
				return c->val;
			c = c->next;
		}
	} else {
		while( c != NULL ) {
			if( val_call2(cmp,key,c->key) == alloc_int(0) )
				return c->val;
			c = c->next;
		}
	}
	return val_null;
}

/**
	$hmem : 'hash -> k:any -> cmp:function:2? -> bool
	<doc>
		Look for the value bound to the key [k] in the hashtable.
		Use the comparison function [cmp] or [$compare] if [null].
		Return true if such value exists, false either.
	</doc>
**/
static value builtin_hmem( value vh, value key, value cmp ) {
	vhash *h;
	hcell *c;
	if( !val_is_null(cmp) )
		val_check_function(cmp,2);
	val_check_kind(vh,k_hash);
	h = val_hdata(vh);
	c = h->cells[val_hash(key) % h->ncells];
	if( val_is_null(cmp) ) {
		while( c != NULL ) {
			if( val_compare(key,c->key) == 0 )
				return val_true;
			c = c->next;
		}
	} else {
		while( c != NULL ) {
			if( val_call2(cmp,key,c->key) == alloc_int(0) )
				return val_true;
			c = c->next;
		}
	}
	return val_false;
}

/**
	$hremove : 'hash -> k:any -> cmp:function:2? -> bool
	<doc>
		Look for the value bound to the key [k] in the hashtable.
		Use the comparison function [cmp] or [$compare] if [null].
		Return true if such value exists and remove it from the hash, false either.
	</doc>
**/
static value builtin_hremove( value vh, value key, value cmp ) {
	vhash *h;
	hcell *c, *prev = NULL;
	int hkey;
	if( !val_is_null(cmp) )
		val_check_function(cmp,2);
	val_check_kind(vh,k_hash);
	h = val_hdata(vh);
	hkey = val_hash(key) % h->ncells;
	c = h->cells[hkey];
	if( val_is_null(cmp) ) {
		while( c != NULL ) {
			if( val_compare(key,c->key) == 0 ) {
				if( prev == NULL )
					h->cells[hkey] = c->next;
				else
					prev->next = c->next;
				h->nitems--;
				return val_true;
			}
			prev = c;
			c = c->next;
		}
	} else {
		while( c != NULL ) {
			if( val_call2(cmp,key,c->key) == alloc_int(0) ) {
				if( prev == NULL )
					h->cells[hkey] = c->next;
				else
					prev->next = c->next;
				h->nitems--;
				return val_true;
			}
			prev = c;
			c = c->next;
		}
	}
	return val_false;
}

/**
	$hset : 'hash -> k:any -> v:any -> cmp:function:2? -> bool
	<doc>
	Set the value bound to key [k] to [v] or add it to the hashtable if not found.
	Return true if the value was added to the hashtable.
	</doc>
**/
static value builtin_hset( value vh, value key, value val, value cmp ) {
	vhash *h;
	hcell *c;
	int hkey;
	if( !val_is_null(cmp) )
		val_check_function(cmp,2);
	val_check_kind(vh,k_hash);
	h = val_hdata(vh);
	hkey = val_hash(key);
	c = h->cells[hkey % h->ncells];
	if( val_is_null(cmp) ) {
		while( c != NULL ) {
			if( val_compare(key,c->key) == 0 ) {
				c->val = val;
				return val_false;
			}
			c = c->next;
		}
	} else {
		while( c != NULL ) {
			if( val_call2(cmp,key,c->key) == alloc_int(0) ) {
				c->val = val;
				return val_false;
			}
			c = c->next;
		}
	}
	if( h->nitems >= (h->ncells << 1) )
		builtin_hresize(vh,alloc_int(h->ncells << 1));
	c = (hcell*)alloc(sizeof(hcell));
	c->hkey = hkey;
	c->key = key;
	c->val = val;
	hkey %= h->ncells;
	c->next = h->cells[hkey];
	h->cells[hkey] = c;
	h->nitems++;
	return val_true;
}

/**
	$hadd : 'hash -> k:any -> v:any -> void
	<doc>
	Add the value [v] with key [k] to the hashtable. Previous binding is masked but not removed.
	</doc>
**/
static value builtin_hadd( value vh, value key, value val ) {
	vhash *h;
	hcell *c;
	int hkey;
	val_check_kind(vh,k_hash);
	h = val_hdata(vh);
	hkey = val_hash(key);
	if( hkey < 0 )
		neko_error();
	if( h->nitems >= (h->ncells << 1) )
		builtin_hresize(vh,alloc_int(h->ncells << 1));
	c = (hcell*)alloc(sizeof(hcell));
	c->hkey = hkey;
	c->key = key;
	c->val = val;
	hkey %= h->ncells;
	c->next = h->cells[hkey];
	h->cells[hkey] = c;
	h->nitems++;
	return val_null;
}

/**
	$hiter : 'hash -> f:function:2 -> void
	<doc>Call the function [f] with every key and value in the hashtable</doc>
**/
static value builtin_hiter( value vh, value f ) {
	int i;
	hcell *c;
	vhash *h;
	val_check_function(f,2);
	val_check_kind(vh,k_hash);
	h = val_hdata(vh);
	for(i=0;i<h->ncells;i++) {
		c = h->cells[i];
		while( c != NULL ) {
			val_call2(f,c->key,c->val);
			c = c->next;
		}
	}
	return val_null;
}

/**
	$hcount : 'hash -> int
	<doc>Return the number of elements in the hashtable</doc>
**/
static value builtin_hcount( value vh ) {
	val_check_kind(vh,k_hash);
	return alloc_int( val_hdata(vh)->nitems );
}

/**
	$hsize : 'hash -> int
	<doc>Return the size of the hashtable</doc>
**/
static value builtin_hsize( value vh ) {
	val_check_kind(vh,k_hash);
	return alloc_int( val_hdata(vh)->ncells );
}

/** <doc><h2>Other Builtins</h2></doc> **/

/**
	$print : any* -> void
	<doc>Can print any value</doc>
**/
static value builtin_print( value *args, int nargs ) {
	buffer b;
	int i;
	if( nargs == 1 && val_is_string(*args) ) {
		val_print(*args);
		return neko_builtins[1];
	}
	b = alloc_buffer(NULL);
	for(i=0;i<nargs;i++)
		val_buffer(b,args[i]);
	val_print(buffer_to_string(b));
	return neko_builtins[1];
}

/**
	$throw : any -> any
	<doc>Throw any value as an exception. Never returns</doc>
**/
static value builtin_throw( value v ) {
	val_throw(v);
	return val_null;
}

/**
	$rethrow : any -> any
	<doc>Throw any value as an exception while keeping previous exception stack. Never returns</doc>
**/
static value builtin_rethrow( value v ) {
	val_rethrow(v);
	return val_null;
}

/**
	$istrue : v:any -> bool
	<doc>Return true if [v] is not [false], not [null] and not 0</doc>
**/
static value builtin_istrue( value f ) {
	return alloc_bool(f != val_false && f != val_null && f != alloc_int(0) && (val_is_int(f) || val_tag(f) != VAL_INT32 || val_int32(f) != 0));
}

/**
	$not : any -> bool
	<doc>Return true if [v] is [false] or [null] or [0]</doc>
**/
static value builtin_not( value f ) {
	return alloc_bool(f == val_false || f == val_null || f == alloc_int(0) || (!val_is_int(f) && val_tag(f) == VAL_INT32 && val_int32(f) == 0));
}

/**
	$typeof : any -> int
	<doc>
		Return the type of a value. The following builtins are defined :
		<ul>
			<li>[$tnull] = 0</li>
			<li>[$tint] = 1</li>
			<li>[$tfloat] = 2</li>
			<li>[$tbool] = 3</li>
			<li>[$tstring] = 4</li>
			<li>[$tobject] = 5</li>
			<li>[$tarray] = 6</li>
			<li>[$tfunction] = 7</li>
			<li>[$tabstract] = 8</li>
		</ul>
	</doc>
**/
static value builtin_typeof( value v ) {
	switch( val_type(v) ) {
	case VAL_INT:
	case VAL_INT32:
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
		return alloc_int(7);
	case VAL_ABSTRACT:
		return alloc_int(8);
	default:
		neko_error();
	}
}

/**
	$compare : any -> any -> int?
	<doc>Compare two values and return 1, -1 or 0. Return [null] if comparison is not possible</doc>
**/
static value builtin_compare( value a, value b ) {
	int r = val_compare(a,b);
	return (r == invalid_comparison)?val_null:alloc_int(r);
}

/**
	$pcompare : any -> any -> int
	<doc>Physically compare two values. Same as [$compare] for integers.</doc>
**/
static value builtin_pcompare( value a, value b ) {
	int_val ia = (int_val)a;
	int_val ib = (int_val)b;
	if( ia > ib )
		return alloc_int(1);
	else if( ia < ib )
		return alloc_int(-1);
	else
		return alloc_int(0);
}

/**
	$excstack : void -> array
	<doc>
	Return the stack between the place the last exception was raised and the place it was catched.
	The stack is composed of the following items :
	<ul>
		<li>[null] when it's a C function</li>
		<li>a string when it's a module without debug informations</li>
		<li>an array of two elements (usually file and line) if debug informations where available</li>
	</ul>
	</doc>
**/
static value builtin_excstack() {
	return NEKO_VM()->exc_stack;
}

/**
	$callstack : void -> array
	<doc>Return the current callstack. Same format as [$excstack]</doc>
**/
static value builtin_callstack() {
	return neko_call_stack(NEKO_VM());
}

/**
    $version : void -> int
	<doc>Return the version of Neko : 135 means 1.3.5</doc>
**/
static value builtin_version() {
	return alloc_int(NEKO_VERSION);
}

/**
	$setresolver : function:2? -> void
	<doc>Set a function to callback with object and field id when an object field is not found.</doc>
**/
static value builtin_setresolver( value f ) {
	neko_vm *vm = NEKO_VM();
	if( val_is_null(f) )
		vm->resolver = NULL;
	else {
		val_check_function(f,2);
		vm->resolver = f;
	}
	return val_null;
}

#define BUILTIN(name,nargs)	\
	alloc_field(neko_builtins[0],val_id(#name),alloc_function(builtin_##name,nargs,"$" #name));

void neko_init_builtins() {
	neko_builtins = alloc_root(2);
	neko_builtins[0] = alloc_object(NULL);
	neko_builtins[1] = alloc_function(builtin_print,VAR_ARGS,"$print");

	BUILTIN(print,VAR_ARGS);

	BUILTIN(array,VAR_ARGS);
	BUILTIN(amake,1);
	BUILTIN(acopy,1);
	BUILTIN(asize,1);
	BUILTIN(asub,3);
	BUILTIN(ablit,5);
	BUILTIN(aconcat,1);

	BUILTIN(smake,1);
	BUILTIN(ssize,1);
	BUILTIN(scopy,1);
	BUILTIN(ssub,3);
	BUILTIN(sget,2);
	BUILTIN(sset,3);
	BUILTIN(sblit,5);
	BUILTIN(sfind,3);

	BUILTIN(new,1);
	BUILTIN(objget,2);
	BUILTIN(objset,3);
	BUILTIN(objcall,3);
	BUILTIN(objfield,2);
	BUILTIN(objremove,2);
	BUILTIN(objfields,1);
	BUILTIN(hash,1);
	BUILTIN(fasthash,1);
	BUILTIN(field,1);
	BUILTIN(objsetproto,2);
	BUILTIN(objgetproto,1);

	BUILTIN(int,1);
	BUILTIN(float,1);
	BUILTIN(string,1);
	BUILTIN(typeof,1);
	BUILTIN(closure,VAR_ARGS);
	BUILTIN(apply,VAR_ARGS);
	BUILTIN(varargs,1);
	BUILTIN(compare,2);
	BUILTIN(pcompare,2);
	BUILTIN(not,1);
	BUILTIN(throw,1);
	BUILTIN(rethrow,1);
	BUILTIN(nargs,1);
	BUILTIN(call,3);
	BUILTIN(isnan,1);
	BUILTIN(isinfinite,1);
	BUILTIN(istrue,1);

	BUILTIN(getkind,1);
	BUILTIN(iskind,2);

	BUILTIN(hnew,1);
	BUILTIN(hget,3);
	BUILTIN(hmem,3);
	BUILTIN(hset,4);
	BUILTIN(hadd,3);
	BUILTIN(hremove,3);
	BUILTIN(hresize,2);
	BUILTIN(hkey,1);
	BUILTIN(hcount,1);
	BUILTIN(hsize,1);
	BUILTIN(hiter,2);

	BUILTIN(iadd,2);
	BUILTIN(isub,2);
	BUILTIN(imult,2);
	BUILTIN(idiv,2);

	BUILTIN(excstack,0);
	BUILTIN(callstack,0);
	BUILTIN(version,0);
	BUILTIN(setresolver,1);
}

/* ************************************************************************ */
