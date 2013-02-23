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

/**
	<doc>
	<h1>Int32</h1>
	<p>
	Int32 api is deprecated as of Neko 2.0, which have native support for Int32.
	</p>
	</doc>
**/

/**
	int32_new : (#int32 | float) -> 'int32
	<doc>Allocate an int32 from any number</doc>
**/
static value int32_new( value v ) {
	val_check(v,number);
	return alloc_int32((int)val_number(v));
}

/**
	int32_to_int : #int32 -> int
	<doc>Return the int value if it can be represented using 31 bits. Error either</doc>
**/
static value int32_to_int( value v ) {
	int i;
	val_check(v,any_int);
	i = val_any_int(v);
	if( need_32_bits(i) )
		neko_error();
	return alloc_int(i);
}

/**
	int32_to_float : #int32 -> float
	<doc>Return the float value of the integer.</doc>
**/
static value int32_to_float( value v ) {
	val_check(v,any_int);	
	return alloc_float(val_any_int(v));
}

/**
	int32_compare : #int32 -> #int32 -> int
	<doc>Compare two integers</doc>
**/
static value int32_compare( value v1, value v2 ) {
	int i1, i2;
	val_check(v1,any_int);
	val_check(v2,any_int);
	i1 = val_any_int(v1);
	i2 = val_any_int(v2);
	if( i1 == i2 )
		return alloc_int(0);
	else if( i1 > i2 )
		return alloc_int(1);
	else
		return alloc_int(-1);
}

#define INT32_OP(op_name,op) \
	static value int32_##op_name( value v1, value v2 ) { \
		int r; \
		val_check(v1,any_int); \
		val_check(v2,any_int); \
		r = val_any_int(v1) op val_any_int(v2); \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,2)

#define INT32_UNOP(op_name,op) \
	static value int32_##op_name( value v ) { \
		int r; \
		val_check(v,any_int); \
		r = op val_any_int(v); \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,1)

#define INT32_OP_ZERO(op_name,op) \
	static value int32_##op_name( value v1, value v2 ) { \
		int d; \
		int r; \
		val_check(v1,any_int); \
		val_check(v2,any_int); \
		d = val_any_int(v2); \
		if( d == 0 ) \
			neko_error(); \
		r = val_any_int(v1) op d; \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,2)

/**
	int32_ushr : #int32 -> #int32 -> #int32
	<doc>Perform unsigned right bits-shifting</doc>
**/
static value int32_ushr( value v1, value v2 ) {
	int r;
	val_check(v1,any_int);
	val_check(v2,any_int);
	r = ((unsigned int)val_any_int(v1)) >> val_any_int(v2);
	return alloc_best_int(r);
}

/** 
	int32_add : #int32 -> #int32 -> #int32
	<doc>Add two integers</doc>
**/
INT32_OP(add,+);
/** 
	int32_sub : #int32 -> #int32 -> #int32
	<doc>Subtract two integers</doc>
**/
INT32_OP(sub,-);
/** 
	int32_mul : #int32 -> #int32 -> #int32
	<doc>Multiply two integers</doc>
**/
INT32_OP(mul,*);
/** 
	int32_div : #int32 -> #int32 -> #int32
	<doc>Divide two integers. Error on division by 0</doc>
**/
INT32_OP_ZERO(div,/);
/** 
	int32_shl : #int32 -> #int32 -> #int32
	<doc>Perform left bit-shifting</doc>
**/
INT32_OP(shl,<<);
/** 
	int32_shr : #int32 -> #int32 -> #int32
	<doc>Perform right bit-shifting</doc>
**/
INT32_OP(shr,>>);
/** 
	int32_mod : #int32 -> #int32 -> #int32
	<doc>Return the modulo of one integer by the other. Error on modulo by 0</doc>
**/
INT32_OP_ZERO(mod,%);
/** 
	int32_neg : #int32 -> #int32
	<doc>Return the negative value of an integer</doc>
**/
INT32_UNOP(neg,-);
/** 
	int32_complement : #int32 -> #int32
	<doc>Return the one-complement bitwised integer</doc>
**/
INT32_UNOP(complement,~);
/** 
	int32_or : #int32 -> #int32 -> #int32
	<doc>Return the bitwise or of two integers</doc>
**/
INT32_OP(or,|);
/** 
	int32_and : #int32 -> #int32 -> #int32
	<doc>Return the bitwise and of two integers</doc>
**/
INT32_OP(and,&);
/** 
	int32_xor : #int32 -> #int32 -> #int32
	<doc>Return the bitwise xor of two integers</doc>
**/
INT32_OP(xor,^);

/**
	int32_address : any -> #int32
	<doc>
	Return the address of the value. 
	The address should not be considered constant. It is not unique
	either unless you are sure you are running on a 32-bit platform.
	</doc>
**/
static value int32_address( value v ) {
	return alloc_best_int((int)(int_val)v);
}

DEFINE_PRIM(int32_new,1);
DEFINE_PRIM(int32_to_int,1);
DEFINE_PRIM(int32_to_float,1);
DEFINE_PRIM(int32_compare,2);
DEFINE_PRIM(int32_ushr,2);
DEFINE_PRIM(int32_address,1);

/* ************************************************************************ */
