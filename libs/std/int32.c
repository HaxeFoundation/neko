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
#include <neko.h>

/**
	<doc>
	<h1>Int32</h1>
	<p>
	Int32 is an abstract type that can be used to store the full 32 bits of an
	integer. The type ['int32] means that the value is a real int32. The type
	[#int32] means [(int | 'int32)] and accept then the both kind of integers.
	</p>
	</doc>
**/

/**
	int32_new : (#int32 | float) -> 'int32
	<doc>Allocate an int32 from any integer or a float</doc>
**/
static value int32_new( value v ) {
	if( val_is_number(v) )
		return alloc_int32((int)val_number(v));
	val_check_kind(v,k_int32);
	return alloc_int32(val_int32(v));
}

/**
	int32_to_int : #int32 -> int
	<doc>Return the int value if it can be represented using 31 bits. Error either</doc>
**/
static value int32_to_int( value v ) {
	int i;
	val_check(v,int32);
	i = val_int32(v);
	if( need_32_bits(i) )
		neko_error();
	return alloc_int(i);
}

/**
	int32_to_float : #int32 -> float
	<doc>Return the float value of the integer.</doc>
**/
static value int32_to_float( value v ) {
	val_check(v,int32);	
	return alloc_float(val_int32(v));
}

/**
	int32_compare : #int32 -> #int32 -> int
	<doc>Compare two integers</doc>
**/
static value int32_compare( value v1, value v2 ) {
	int i1, i2;
	val_check(v1,int32);
	val_check(v2,int32);
	i1 = val_int32(v1);
	i2 = val_int32(v2);
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
		val_check(v1,int32); \
		val_check(v2,int32); \
		r = val_int32(v1) op val_int32(v2); \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,2)

#define INT32_UNOP(op_name,op) \
	static value int32_##op_name( value v ) { \
		int r; \
		val_check(v,int32); \
		r = op val_int32(v); \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,1)

#define INT32_OP_ZERO(op_name,op) \
	static value int32_##op_name( value v1, value v2 ) { \
		int d; \
		int r; \
		val_check(v1,int32); \
		val_check(v2,int32); \
		d = val_int32(v2); \
		if( d == 0 ) \
			neko_error(); \
		r = val_int32(v1) op d; \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,2)

/**
	int32_ushr : #int32 -> #int32 -> #int32
	<doc>Perform unsigned right bits-shifting</doc>
**/
static value int32_ushr( value v1, value v2 ) {
	int r;
	val_check(v1,int32);
	val_check(v2,int32);
	r = ((unsigned int)val_int32(v1)) >> val_int32(v2);
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
	return alloc_best_int((int_val)v);
}

DEFINE_PRIM(int32_new,1);
DEFINE_PRIM(int32_to_int,1);
DEFINE_PRIM(int32_to_float,1);
DEFINE_PRIM(int32_compare,2);
DEFINE_PRIM(int32_ushr,2)
DEFINE_PRIM(int32_address,1);

/* ************************************************************************ */
