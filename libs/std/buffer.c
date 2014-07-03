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
	<h1>Buffer</h1>
	<p>
	A buffer can store any value as a string and will only allocate the total
	needed space when requested. It makes a copy of each value when stored so
	modifying them after is not a problem.
	</p>
	</doc>
**/

DEFINE_KIND(k_buffer);

/**
	buffer_new : void -> 'buffer
	<doc>Allocate a new empty buffer</doc>
**/
static value buffer_new() {
	buffer b = alloc_buffer(NULL);
	return alloc_abstract(k_buffer,b);
}

/**
	buffer_add : 'buffer -> any -> void
	<doc>Add a value to a buffer</doc>
**/
static value buffer_add( value b, value v ) {
	val_check_kind(b,k_buffer);
	val_buffer( (buffer)val_data(b), v );
	return val_true;
}

/**
	buffer_add_char : 'buffer -> c:int -> void
	<doc>Add a single char to a buffer. Error if [c] is not in the 0..255 range</doc>
**/
static value buffer_add_char( value b, value c ) {
	val_check_kind(b,k_buffer);
	val_check(c,int);
	if( val_int(c) < 0 || val_int(c) > 255 )
		neko_error();	
	buffer_append_char( (buffer)val_data(b), (char)(unsigned char)val_int(c) );
	return val_true;
}

/**
	buffer_add_sub : 'buffer -> s:string -> p:int -> l:int -> void
	<doc>Add [l] characters of the string [s] starting at position [p]. An error occurs if out of string bounds.</doc>
**/
static value buffer_add_sub( value b, value v, value p, value l ) {
	val_check_kind(b,k_buffer);
	val_check(v,string);
	val_check(p,int);
	val_check(l,int);	
	if( val_int(p) < 0 || val_int(l) < 0 )
		neko_error();
	if( val_strlen(v) < val_int(p) || val_strlen(v) < val_int(p) + val_int(l) )
		neko_error();
	buffer_append_sub( (buffer)val_data(b), val_string(v) + val_int(p) , val_int(l) );
	return val_true;
}

/**
	buffer_string : 'buffer -> string
	<doc>Build and return the string built with the buffer</doc>
**/
static value buffer_string( value b ) {
	val_check_kind(b,k_buffer);
	return buffer_to_string( (buffer)val_data(b) );
}

/**
	buffer_reset : 'buffer -> void
	<doc>Make the buffer empty</doc>
**/
static value buffer_reset( value b ) {
	val_check_kind(b,k_buffer);
	val_data(b) = alloc_buffer(NULL);
	return val_true;
}

/**
	buffer_get_length : 'buffer -> int
	<doc>Return the number of bytes currently stored into the buffer</doc>
**/
static value buffer_get_length( value b ) {
	val_check_kind(b,k_buffer);
	return alloc_best_int(buffer_length((buffer)val_data(b)));
}

DEFINE_PRIM(buffer_new,0);
DEFINE_PRIM(buffer_add,2);
DEFINE_PRIM(buffer_add_char,2);
DEFINE_PRIM(buffer_add_sub,4);
DEFINE_PRIM(buffer_string,1);
DEFINE_PRIM(buffer_reset,1);
DEFINE_PRIM(buffer_get_length,1);

/* ************************************************************************ */
