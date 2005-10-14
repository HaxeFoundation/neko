/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
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
#include <neko.h>

static value int32_new( value v ) {	
	val_check(v,int32);	
	return alloc_int32(val_int32(v));
}

static value int32_to_int( value v ) {
	int i;
	val_check(v,int32);
	i = val_int32(v);
	if( need_32_bits(i) )
		type_error();
	return alloc_int(i);
}

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
			type_error(); \
		r = val_int32(v1) op d; \
		return alloc_best_int(r); \
	} \
	DEFINE_PRIM(int32_##op_name,2)

static value int32_ushr( value v1, value v2 ) {
	int r;
	val_check(v1,int32);
	val_check(v2,int32);
	r = ((unsigned int)val_int32(v1)) >> val_int32(v2);
	return alloc_best_int(r);
}

INT32_OP(add,+);
INT32_OP(sub,-);
INT32_OP(mul,*);
INT32_OP_ZERO(div,/);
INT32_OP(shl,<<);
INT32_OP(shr,>>);
INT32_OP_ZERO(mod,%);
INT32_UNOP(neg,-);
INT32_UNOP(complement,~);
INT32_OP(or,|);
INT32_OP(and,&);
INT32_OP(xor,^);

DEFINE_PRIM(int32_new,1);
DEFINE_PRIM(int32_to_int,1);
DEFINE_PRIM(int32_compare,2);
DEFINE_PRIM(int32_ushr,2)

/* ************************************************************************ */
