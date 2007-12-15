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
#include <stdlib.h>
#include <math.h>

/**
	<doc>
	<h1>Math</h1>
	<p>
	Mathematical functions
	</p>
	</doc>
**/

#if defined(NEKO_VCC) && !defined(NEKO_INSTALLER)
	long _ftol( double f );
	long _ftol2( double f) { return _ftol(f); };
#endif

#define MATH_PRIM(f) \
	value math_##f( value n ) { \
		val_check(n,number); \
		return alloc_float( f( val_number(n) ) ); \
	} \
	DEFINE_PRIM(math_##f,1)

/**
	math_atan2 : number -> number -> float
	<doc>Return atan2 calculus</doc>
**/
static value math_atan2( value a, value b ) {
	val_check(a,number);
	val_check(b,number);
	return alloc_float( atan2(val_number(a),val_number(b)) );
}

/**
	math_pow : number -> number -> float
	<doc>Return power calculus</doc>
**/
static value math_pow( value a, value b ) {
	tfloat r;
	val_check(a,number);
	val_check(b,number);
	r = (tfloat)pow(val_number(a),val_number(b));
	if( (int)r == r && fabs(r) < (1 << 30) )
		return alloc_int((int)r);
	return alloc_float(r);
}

/**
	math_abs : number -> number
	<doc>Return absolute value of a number</doc>
**/
static value math_abs( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return alloc_int( abs(val_int(n)) );
	case VAL_FLOAT:
		return alloc_float( fabs(val_float(n)) ); 
	default:
		neko_error();
	}
}

/**
	math_ceil : number -> int
	<doc>Return rounded-up integer</doc>
**/
static value math_ceil( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return n;
	case VAL_FLOAT:
		return alloc_int( (int)ceil(val_float(n)) );
	default:
		neko_error();
	}
}

/**
	math_floor : number -> int
	<doc>Return rounded-down integer</doc>
**/
static value math_floor( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return n;
	case VAL_FLOAT:
		return alloc_int( (int)floor(val_float(n)) );
	default:
		neko_error();
	}
}

/**
	math_round : number -> int
	<doc>Return nearest integer</doc>
**/
static value math_round( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
		return n;
	case VAL_FLOAT:
		return alloc_int( (int)floor(val_float(n) + 0.5) );
	default:
		neko_error();
	}
}

#define PI 3.1415926535897932384626433832795

/**
	math_pi : void -> float
	<doc>Return the value of PI</doc>
**/
static value math_pi() {
	return alloc_float(PI);
}

/**
	math_sqrt : number -> float
	<doc>Return the square-root</doc>
**/
MATH_PRIM(sqrt);
/**
	math_atan : number -> float
	<doc>Return the arc-tangent</doc>
**/
MATH_PRIM(atan);
/**
	math_cos : number -> float
	<doc>Return the cosinus</doc>
**/
MATH_PRIM(cos);
/**
	math_sin : number -> float
	<doc>Return the sinus</doc>
**/
MATH_PRIM(sin);
/**
	math_tan : number -> float
	<doc>Return the tangent</doc>
**/
MATH_PRIM(tan);
/**
	math_log : number -> float
	<doc>Return the logarithm</doc>
**/
MATH_PRIM(log);
/**
	math_exp : number -> float
	<doc>Return the exponant</doc>
**/
MATH_PRIM(exp);
/**
	math_acos : number -> float
	<doc>Return the arc-cosinus</doc>
**/
MATH_PRIM(acos);
/**
	math_asin : number -> float
	<doc>Return the arc-sinus</doc>
**/
MATH_PRIM(asin);

DEFINE_PRIM(math_pi,0);
DEFINE_PRIM(math_atan2,2);
DEFINE_PRIM(math_pow,2);
DEFINE_PRIM(math_abs,1);
DEFINE_PRIM(math_ceil,1);
DEFINE_PRIM(math_floor,1);
DEFINE_PRIM(math_round,1);

/* ************************************************************************ */
