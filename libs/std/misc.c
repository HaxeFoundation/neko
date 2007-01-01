/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
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
#include <neko_vm.h>

/**
	<doc>
	<h1>Misc</h1>
	<p>
	Misc. functions for different usages.
	</p>
	</doc>
**/

/**
	float_bytes : number -> bigendian:bool -> string
	<doc>Returns the 4 bytes representation of the number as an IEEE 32-bit float</doc>
**/
static value float_bytes( value n, value be ) {
	float f;
	val_check(n,number);
	val_check(be,bool);
	f = (float)val_number(n);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0];	c[0] = c[3]; c[3] = tmp;
		tmp = c[1];	c[1] = c[2]; c[2] = tmp;
	}
	return copy_string((char *)&f,4);
}

/**
	double_bytes : number -> bigendian:bool -> string
	<doc>Returns the 8 bytes representation of the number as an IEEE 64-bit float</doc>
**/
static value double_bytes( value n, value be ) {
	double f;
	val_check(n,number);
	val_check(be,bool);
	f = (double)val_number(n);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0]; c[0] = c[7]; c[7] = tmp;
		tmp = c[1];	c[1] = c[6]; c[6] = tmp;
		tmp = c[2]; c[2] = c[5]; c[5] = tmp;
		tmp = c[3];	c[3] = c[4]; c[4] = tmp;
	}
	return copy_string((char*)&f,8);
}

/**
	float_of_bytes : string -> bigendian:bool -> float
	<doc>Returns a float from a 4 bytes IEEE 32-bit representation</doc>
**/
static value float_of_bytes( value s, value be ) {
	float f;
	val_check(s,string);
	val_check(be,bool);
	if( val_strlen(s) != 4 )
		neko_error();
	f = *(float*)val_string(s);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0];	c[0] = c[3]; c[3] = tmp;
		tmp = c[1];	c[1] = c[2]; c[2] = tmp;
	}
	return alloc_float(f);
}

/**
	double_of_bytes : string -> bigendian:bool -> float
	<doc>Returns a float from a 8 bytes IEEE 64-bit representation</doc>
**/
static value double_of_bytes( value s, value be ) {
	double f;
	val_check(s,string);
	val_check(be,bool);
	if( val_strlen(s) != 8 )
		neko_error();
	f = *(double*)val_string(s);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0]; c[0] = c[7]; c[7] = tmp;
		tmp = c[1];	c[1] = c[6]; c[6] = tmp;
		tmp = c[2]; c[2] = c[5]; c[5] = tmp;
		tmp = c[3];	c[3] = c[4]; c[4] = tmp;
	}
	return alloc_float(f);
}

/**
	run_gc : major:bool -> void
	<doc>Run the Neko garbage collector</doc>
**/
static value run_gc( value b ) {
	val_check(b,bool);
	if( val_bool(b) )
		neko_gc_major();
	else
		neko_gc_loop();
	return val_null;
}

/**
	gc_stats : void -> { heap => int, free => int }
	<doc>Return the size of the GC heap and the among of free space, in bytes</doc>
**/
static value gc_stats() {
	int heap, free;
	value o;
	neko_gc_stats(&heap,&free);
	o = alloc_object(NULL);
	alloc_field(o,val_id("heap"),alloc_int(heap));
	alloc_field(o,val_id("free"),alloc_int(free));
	return o;
}

/**
	enable_jit : bool -> void
	<doc>Enable or disable the JIT.</doc>
**/
static value enable_jit( value b ) {	
	val_check(b,bool);
	neko_vm_jit(neko_vm_current(),val_bool(b));
	return val_null;
}

/**
	test : void -> void
	<doc>The test function, to check that library is reachable and correctly linked</doc>
**/
static value test() {
	val_print(alloc_string("Calling a function inside std library...\n"));
	return val_null;
}

/**
	print_redirect : function:1? -> void
	<doc>
	Set a redirection function for all printed values. 
	Setting it to null will cancel the redirection and restore previous printer.
	</doc>
**/

static void print_callback( const char *s, int size, void *f ) {	
	val_call1(f,copy_string(s,size));
}

static value print_redirect( value f ) {
	neko_vm *vm = neko_vm_current();
	if( val_is_null(f) ) {
		neko_vm_redirect(vm,NULL,NULL);
		return val_true;
	}
	val_check_function(f,1);
	neko_vm_redirect(vm,print_callback,f);
	return val_true;
}


DEFINE_PRIM(float_bytes,2);
DEFINE_PRIM(double_bytes,2);
DEFINE_PRIM(float_of_bytes,2);
DEFINE_PRIM(double_of_bytes,2);
DEFINE_PRIM(run_gc,1);
DEFINE_PRIM(gc_stats,0);
DEFINE_PRIM(enable_jit,1);
DEFINE_PRIM(test,0);
DEFINE_PRIM(print_redirect,1);

/* ************************************************************************ */
