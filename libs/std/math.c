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
	if( (int)r == r && fabs(r) < (1 << 31) )
		return alloc_best_int((int)r);
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
	case VAL_INT32:
		return alloc_int32( abs(val_int32(n)) );
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
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		return alloc_best_int( (int)ceil(val_float(n)) );
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
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		return alloc_best_int( (int)floor(val_float(n)) );
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
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		return alloc_best_int( (int)floor(val_float(n) + 0.5) );
	default:
		neko_error();
	}
}

/**
	math_fceil : number -> number
	<doc>Return rounded-up float without integer overflow</doc>
**/
static value math_fceil( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		return alloc_float( ceil(val_float(n)) );
	default:
		neko_error();
	}
}

/**
	math_ffloor : number -> number
	<doc>Return rounded-down float without integer overflow</doc>
**/
static value math_ffloor( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		return alloc_float( floor(val_float(n)) );
	default:
		neko_error();
	}
}

/**
	math_fround : number -> number
	<doc>Return rounded float without integer overflow</doc>
**/
static value math_fround( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		return alloc_float( floor(val_float(n) + 0.5) );
	default:
		neko_error();
	}
}

/**
	math_int : number -> int
	<doc>Return integer rounded down towards 0</doc>
**/
static value math_int( value n ) {
	switch( val_type(n) ) {
	case VAL_INT:
	case VAL_INT32:
		return n;
	case VAL_FLOAT:
		{
			tfloat v = val_float(n);
			return alloc_best_int( (int)((n < 0) ? ceil(v) : floor(v)) );
		}
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
DEFINE_PRIM(math_fceil,1);
DEFINE_PRIM(math_ffloor,1);
DEFINE_PRIM(math_fround,1);
DEFINE_PRIM(math_int,1);

/* ************************************************************************ */
