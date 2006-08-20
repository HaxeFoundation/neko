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
	<h1>Misc</h1>
	<p>
	Misc. functions for different usages.
	</p>
	</doc>
**/

/**
	float_bytes : number -> string
	<doc>Returns the 4 bytes representation of the number as an IEEE 32-bit float</doc>
**/
static value float_bytes( value n ) {
	float f;
	val_check(n,number);
	f = (float)val_number(n);
	return copy_string((char *)&f,4);
}

/**
	double_bytes : number -> string
	<doc>Returns the 8 bytes representation of the number as an IEEE 64-bit float</doc>
**/
static value double_bytes( value n ) {
	double f;
	val_check(n,number);
	f = (double)val_number(n);
	return copy_string((char *)&f,8);
}

/**
	float_of_bytes : string -> float
	<doc>Returns a float from a 4 bytes IEEE 32-bit representation</doc>
**/
static value float_of_bytes( value s ) {
	val_check(s,string);
	if( val_strlen(s) != 4 )
		neko_error();	
	return alloc_float( *(float*)val_string(s) );
}

/**
	double_of_bytes : string -> float
	<doc>Returns a float from a 8 bytes IEEE 64-bit representation</doc>
**/
static value double_of_bytes( value s ) {
	val_check(s,string);
	if( val_strlen(s) != 8 )
		neko_error();	
	return alloc_float( *(double*)val_string(s) );
}

DEFINE_PRIM(float_bytes,1);
DEFINE_PRIM(double_bytes,1);
DEFINE_PRIM(float_of_bytes,1);
DEFINE_PRIM(double_of_bytes,1);

/* ************************************************************************ */
