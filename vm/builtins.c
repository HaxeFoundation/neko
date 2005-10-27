/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include <stdlib.h>
#include "neko.h"
#include "objtable.h"
#include "vm.h"

#ifdef _WIN32
	long _ftol( double f );
	long _ftol2( double f) { return _ftol(f); };
#endif

extern value *neko_builtins;

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

static value builtin_new( value o ) {
	if( !val_is_null(o) && !val_is_object(o) )
		neko_error();
	return alloc_object(o);
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
	val_check(size,int);
	s = val_int(size);
	a = alloc_array(s);
	for(i=0;i<s;i++)
		val_array_ptr(a)[i] = val_null;
	return a;
}

static value builtin_acopy( value a ) {
	int i;
	value a2;
	val_check(a,array);
	a2 = alloc_array(val_array_size(a));
	for(i=0;i<val_array_size(a);i++)
		val_array_ptr(a2)[i] = val_array_ptr(a)[i];
	return a2;
}

static value builtin_asize( value a ) {
	val_check(a,array);
	return alloc_int( val_array_size(a) );
}

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
		return val_null;
	a2 = alloc_array(ll);
	for(i=0;i<ll;i++)
		val_array_ptr(a2)[i] = val_array_ptr(a)[pp+i];
	return a2;
}

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
		return val_null;
	memcpy(val_array_ptr(dst)+dpp,val_array_ptr(src)+spp,ll * sizeof(value));
	return val_true;
}

static value builtin_smake( value l ) {
	val_check(l,int);
	return alloc_empty_string( val_int(l) );
}

static value builtin_ssize( value s ) {
	val_check(s,string);
	return alloc_int(val_strlen(s));
}

static value builtin_scopy( value s ) {
	val_check(s,string);
	return copy_string( val_string(s), val_strlen(s) );
}

static value builtin_ssub( value s, value p, value l ) {
	int pp , ll;
	val_check(s,string);
	val_check(p,int);
	val_check(l,int);
	pp = val_int(p);
	ll = val_int(l);
	if( pp < 0 || ll < 0 || pp + ll < 0 || pp + ll > val_strlen(s) )
		return val_null;
	return copy_string( val_string(s) + pp , ll );
}

static value builtin_sget( value s, value p ) {
	int pp;
	val_check(s,string);
	val_check(p,int);
	pp = val_int(p);
	if( pp < 0 || pp >= val_strlen(s) )
		return val_null;
	return alloc_int( val_string(s)[pp] );
}

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
		return val_null;
	memcpy(val_string(dst)+dpp,val_string(src)+spp,ll);
	return val_true;
}

static value builtin_throw( value v ) {
	val_throw(v);
	return val_null;
}

static value builtin_rethrow( value v ) {
	val_rethrow(v);
	return val_null;
}

static value builtin_nargs( value f ) {
	if( !val_is_function(f) )
		neko_error();
	return alloc_int( val_fun_nargs(f) );
}

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

static value builtin_iadd( value a, value b ) {
	return alloc_int( val_int(a) + val_int(b) );
}

static value builtin_isub( value a, value b ) {
	return alloc_int( val_int(a) - val_int(b) );
}

static value builtin_imult( value a, value b ) {
	return alloc_int( val_int(a) * val_int(b) );
}

static value builtin_idiv( value a, value b ) {
	if( b == (value)1 )
		failure("Integer division by 0");
	return alloc_int( val_int(a) / val_int(b) );
}

typedef union {
	double d;
	union {
		unsigned int l;
		unsigned int h;
	} i;
} qw;

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

static value builtin_istrue( value f ) {
	return alloc_bool(f != val_false && f != val_null && f != alloc_int(0));
}

static value builtin_objget( value o, value f ) {
	if( !val_is_object(o) )
		return val_null; // keep dot-access semantics
	val_check(f,int);
	return val_field(o,val_int(f));
}

static value builtin_objset( value o, value f, value v ) {
	if( !val_is_object(o) )
		return val_null; // keep dot-access semantics
	val_check(f,int);
	alloc_field(o,val_int(f),v);
	return v;
}

static value builtin_objcall( value o, value f, value args ) {
	if( !val_is_object(o) )
		return val_null; // keep dot-access semantics
	val_check(f,int);
	val_check(args,array);
	return val_ocallN(o,val_int(f),val_array_ptr(args),val_array_size(args));
}

static value builtin_objfield( value o, value f ) {
	return alloc_bool( val_is_object(o) && val_is_int(f) && otable_find(((vobject*)o)->table, val_int(f)) != NULL );
}

static value builtin_objremove( value o, value f ) {
	val_check(o,object);
	val_check(f,int);
	return alloc_bool( otable_remove(((vobject*)o)->table,val_int(f)) );
}

static void builtin_objfields_rec( value d, field id, void *a ) {
	*((*(value**)a)++) = alloc_int((int)id);
}

static value builtin_objfields( value o ) {
	value a;
	value *aptr;
	objtable t;
	val_check(o,object);
	t = ((vobject*)o)->table;
	a = alloc_array(otable_count(t));
	aptr = val_array_ptr(a);
	otable_iter(t,builtin_objfields_rec,&aptr);
	return a;
}

static value builtin_hash( value f ) {
	val_check(f,string);
	return alloc_int( (int)val_id(val_string(f)) );
}

static value builtin_field( value f ) {
	val_check(f,int);
	return val_field_name(val_int(f));
}

static value builtin_int( value f ) {
	if( val_is_string(f) ) {
		char *c = val_string(f);
		if( val_strlen(f) >= 2 && c[0] == '0' && c[1] == 'x' ) {
			int h = 0;
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
					return alloc_int(0);
			}
			return alloc_int(h);
		}
		return alloc_int( atoi(val_string(f)) );
	}
	if( val_is_number(f) )
		return alloc_int( (int)val_number(f) );
	return val_null;
}

static value builtin_float( value f ) {
	if( val_is_string(f) )
		return alloc_float( atof(val_string(f)) );
	if( val_is_float(f) )
		return alloc_float( val_number(f) );
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
		return alloc_int(7);
	case VAL_ABSTRACT:
		return alloc_int(8);
	default:
		neko_error();
	}
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
	return alloc_apply(fargs-nargs,env);
}

static value builtin_compare( value a, value b ) {
	int r = val_compare(a,b);
	return (r == invalid_comparison)?val_null:alloc_int(r);
}

static value builtin_not( value f ) {
	return alloc_bool(f == val_false || f == val_null || f == alloc_int(0));
}

static value builtin_string( value v ) {
	buffer b = alloc_buffer(NULL);
	val_buffer(b,v);
	return buffer_to_string(b);
}

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

static value builtin_args() {
	return NEKO_VM()->args;
}

static value builtin_hkey( value v ) {
	return alloc_int(val_hash(v));
}

#define HASH_DEF_SIZE 7

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
	for(i=0;i<h->ncells;i++)
		add_rec(cc,nsize,h->cells[i]);
	h->cells = cc;
	h->ncells = nsize;
	return val_true;
}

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
	return val_true;
}

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

static value builtin_hcount( value vh ) {
	val_check_kind(vh,k_hash);
	return alloc_int( val_hdata(vh)->nitems );
}

static value builtin_hsize( value vh ) {
	val_check_kind(vh,k_hash);
	return alloc_int( val_hdata(vh)->ncells );
}

static value builtin_excstack() {
	return NEKO_VM()->exc_stack;
}

static value builtin_callstack() {
	return neko_call_stack(NEKO_VM());
}

#define BUILTIN(name,nargs)	\
	alloc_field(neko_builtins[0],val_id(#name),alloc_function(builtin_##name,nargs,"$" #name));	

void neko_init_builtins() {
	neko_builtins = alloc_root(1);
	neko_builtins[0] = alloc_object(NULL);

	BUILTIN(print,VAR_ARGS);
	BUILTIN(args,0);
	
	BUILTIN(array,VAR_ARGS);
	BUILTIN(amake,1);
	BUILTIN(acopy,1);
	BUILTIN(asize,1);
	BUILTIN(asub,3);
	BUILTIN(ablit,5);

	BUILTIN(smake,1);
	BUILTIN(ssize,1);
	BUILTIN(scopy,1);
	BUILTIN(ssub,3);
	BUILTIN(sget,2);
	BUILTIN(sset,3);
	BUILTIN(sblit,5);

	BUILTIN(new,1);	
	BUILTIN(objget,2);
	BUILTIN(objset,3);
	BUILTIN(objcall,3);
	BUILTIN(objfield,2);
	BUILTIN(objremove,2);
	BUILTIN(objfields,1);
	BUILTIN(hash,1);
	BUILTIN(field,1);

	BUILTIN(int,1);
	BUILTIN(float,1);
	BUILTIN(string,1);
	BUILTIN(typeof,1);
	BUILTIN(closure,VAR_ARGS);
	BUILTIN(apply,VAR_ARGS);
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
}

/* ************************************************************************ */
