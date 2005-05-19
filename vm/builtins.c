/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <time.h>
#include "neko.h"
#include "load.h"

value *builtins = NULL;

#define BUILTIN(name,nargs)	builtins[p++] = alloc_function(builtin_##name,nargs)

static value builtin_print( value *args, int nargs ) {
/*	buffer b = alloc_buffer(NULL);
	int i;
	for(i=0;i<nargs;i++)
		val_buffer(b,args[i]);
	val_print(buffer_to_string(b));
*/	return val_true;
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
//	if( !val_is_array(a) || !val_is_int(p) )
//		val_throw(alloc_string("aget"));
	pp = val_int(p);
//	if( pp < 0 || pp >= val_array_size(a) )
//		val_throw(alloc_string("aget"));
	return val_array_ptr(a)[pp];
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
/*	BUILTIN(aset,3);
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
*/
}

void free_builtins() {
	free_root(builtins);
}

/* ************************************************************************ */
