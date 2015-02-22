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
#include <neko.h>
#include <stdio.h>

/**
	<doc>
	<h1>Int64</h1>
	<p>
	The Int64 API is new on Neko 2.1, and provides an API to manipulate 64-bit integers
	</p>
	</doc>
**/

/**
	int64_new : (#int64 | float) -> 'int64
	<doc>Allocate an int64 from any number</doc>
**/
static value int64_new( value v ) {
	double f;
	val_check(v,number);
	f = val_number(v);
	return alloc_int64((neko_int64)f);
}

/**
	int64_make : #int -> #int -> #int64
	<doc>Allocate an int64 from both the high and low parts</doc>
**/
static value int64_make( value high, value low ) {
	val_check(high,any_int);
	val_check(low,any_int);
	return alloc_int64( (((neko_int64) (unsigned int) val_any_int(high)) << 32) | ((unsigned int) val_any_int(low)) );
}

/**
	int64_to_int : #int64 -> int
	<doc>Return the int value if it can be represented using 32 bits. Error either</doc>
**/
static value int64_to_int( value v ) {
	neko_int64 i;
	val_check(v,any_int64);
	i = val_any_int64(v);
	if( need_64_bits(i) )
		neko_error();
	return alloc_best_int(i);
}

/**
	int64_to_float : #int64 -> float
	<doc>Return the float value of the integer.</doc>
**/
static value int64_to_float( value v ) {
	val_check(v,any_int64);	
	return alloc_float((double) val_any_int64(v));
}

/**
	int64_compare : #int64 -> #int64 -> int
	<doc>Compare two integers</doc>
**/
static value int64_compare( value v1, value v2 ) {
	neko_int64 i1, i2;
	val_check(v1,any_int64);
	val_check(v2,any_int64);
	i1 = val_any_int64(v1);
	i2 = val_any_int64(v2);
	if( i1 == i2 )
		return alloc_int(0);
	else if( i1 > i2 )
		return alloc_int(1);
	else
		return alloc_int(-1);
}

/**
	int64_ucompare : #int64 -> #int64 -> int
	<doc>Unsigned compare two integers</doc>
**/
static value int64_ucompare( value v1, value v2 ) {
	neko_uint64 i1, i2;
	val_check(v1,any_int64);
	val_check(v2,any_int64);
	i1 = val_any_int64(v1);
	i2 = val_any_int64(v2);
	if( i1 == i2 )
		return alloc_int(0);
	else if( i1 > i2 )
		return alloc_int(1);
	else
		return alloc_int(-1);
}


#define INT64_OP(op_name,op) \
	static value int64_##op_name( value v1, value v2 ) { \
		neko_int64 r; \
		val_check(v1,any_int64); \
		val_check(v2,any_int64); \
		r = val_any_int64(v1) op val_any_int64(v2); \
		return alloc_best_int64(r); \
	} \
	DEFINE_PRIM(int64_##op_name,2)

#define INT64_UNOP(op_name,op) \
	static value int64_##op_name( value v ) { \
		neko_int64 r; \
		val_check(v,any_int64); \
		r = op val_any_int64(v); \
		return alloc_best_int64(r); \
	} \
	DEFINE_PRIM(int64_##op_name,1)

#define INT64_OP_ZERO(op_name,op) \
	static value int64_##op_name( value v1, value v2 ) { \
		neko_int64 d; \
		neko_int64 r; \
		val_check(v1,any_int64); \
		val_check(v2,any_int64); \
		d = val_any_int64(v2); \
		if( d == 0LL ) \
			neko_error(); \
		r = val_any_int64(v1) op d; \
		return alloc_best_int64(r); \
	} \
	DEFINE_PRIM(int64_##op_name,2)

/**
	int64_ushr : #int64 -> #int64 -> #int64
	<doc>Perform unsigned right bits-shifting</doc>
**/
static value int64_ushr( value v1, value v2 ) {
	neko_int64 r;
	val_check(v1,any_int64);
	val_check(v2,any_int64);
	r = ((neko_uint64)val_any_int64(v1)) >> val_any_int64(v2);
	return alloc_best_int64(r);
}

/** 
	int64_add : #int64 -> #int64 -> #int64
	<doc>Add two integers</doc>
**/
INT64_OP(add,+);
/** 
	int64_sub : #int64 -> #int64 -> #int64
	<doc>Subtract two integers</doc>
**/
INT64_OP(sub,-);
/** 
	int64_mul : #int64 -> #int64 -> #int64
	<doc>Multiply two integers</doc>
**/
INT64_OP(mul,*);
/** 
	int64_div : #int64 -> #int64 -> #int64
	<doc>Divide two integers. Error on division by 0</doc>
**/
INT64_OP_ZERO(div,/);
/** 
	int64_shl : #int64 -> #int64 -> #int64
	<doc>Perform left bit-shifting</doc>
**/
INT64_OP(shl,<<);
/** 
	int64_shr : #int64 -> #int64 -> #int64
	<doc>Perform right bit-shifting</doc>
**/
INT64_OP(shr,>>);
/** 
	int64_mod : #int64 -> #int64 -> #int64
	<doc>Return the modulo of one integer by the other. Error on modulo by 0</doc>
**/
INT64_OP_ZERO(mod,%);
/** 
	int64_neg : #int64 -> #int64
	<doc>Return the negative value of an integer</doc>
**/
INT64_UNOP(neg,-);
/** 
	int64_complement : #int64 -> #int64
	<doc>Return the one-complement bitwised integer</doc>
**/
INT64_UNOP(complement,~);
/** 
	int64_or : #int64 -> #int64 -> #int64
	<doc>Return the bitwise or of two integers</doc>
**/
INT64_OP(or,|);
/** 
	int64_and : #int64 -> #int64 -> #int64
	<doc>Return the bitwise and of two integers</doc>
**/
INT64_OP(and,&);
/** 
	int64_xor : #int64 -> #int64 -> #int64
	<doc>Return the bitwise xor of two integers</doc>
**/
INT64_OP(xor,^);

/**
	int64_address : any -> #int64
	<doc>
	Return the address of the value. 
	The address should not be considered constant.
	</doc>
**/
static value int64_address( value v ) {
	return alloc_int64((neko_int64)(int_val)v);
}

/**
	int64_address : abstract -> #int64
	<doc>
	Return the address of the abstract value. 
	</doc>
**/
static value int64_abstract_address( value v ) {
	val_check(v,abstract);
	return alloc_int64( (neko_int64)(int_val)val_data(v) );
}

/**
	int64_to_string : #int64 -> string
	<doc>
	Return the string representation of the int64 value
	</doc>
**/
static value int64_to_string( value v ) {
	char str[25];
	val_check(v,any_int64);
	sprintf(str, "%lld", (val_any_int64(v)));
	return alloc_string(str);
}

/**
	int64_to_hex : #int64 -> string
	<doc>
	Return the hexadecimal representation of the int64 value
	</doc>
**/
static value int64_to_hex( value v ) {
	char str[19];
	sprintf(str, "0x%016llx", val_any_int64(v));
	return alloc_string(str);
}

/**
 private api: avoids a new allocation by changing the high and low parts of the int64
 this is used by some Haxe private APIs
**/
static value int64_replace( value int64, value high, value low ) {
	vint64 *v;
	val_check(int64,int64);
	val_check(high,any_int);
	val_check(low,any_int);

	v = (vint64 *) int64;
	v->i = (((neko_int64) val_any_int(high)) << 32) | (val_any_int(low));

	return int64;
}

/**
	int64_get_high : #int64 -> int
	<doc>
	Return the high 32-bit word 
	</doc>
**/
static value int64_get_high( value int64 ) {
	int r;
	val_check(int64,any_int64);
	r = (int) ((((neko_uint64) val_any_int64(int64) ) >> 32) & 0xFFFFFFFFLL);
	return alloc_best_int(r);
}

/**
	int64_get_low : #int64 -> int
	<doc>
	Return the low 32-bit word 
	</doc>
**/
static value int64_get_low( value int64 ) {
	int r;
	val_check(int64,any_int64);
	r = (int) ((val_any_int64(int64)) & 0xFFFFFFFFLL);
	return alloc_best_int(r);
}

DEFINE_PRIM(int64_new,1);
DEFINE_PRIM(int64_make,2);
DEFINE_PRIM(int64_to_int,1);
DEFINE_PRIM(int64_to_float,1);
DEFINE_PRIM(int64_compare,2);
DEFINE_PRIM(int64_ucompare,2);
DEFINE_PRIM(int64_ushr,2);
DEFINE_PRIM(int64_address,1);
DEFINE_PRIM(int64_abstract_address,1);
DEFINE_PRIM(int64_to_string,1);
DEFINE_PRIM(int64_to_hex,1);
DEFINE_PRIM(int64_replace,3);
DEFINE_PRIM(int64_get_high,1);
DEFINE_PRIM(int64_get_low,1);

/* ************************************************************************ */

