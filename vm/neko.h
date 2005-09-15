/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
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
#ifndef _NEKO_H
#define _NEKO_H

#ifndef NULL
#	define NULL			0
#endif

#ifdef _WIN32
#include <basetsd.h>
typedef INT_PTR		int_val;
typedef UINT_PTR	uint_val;
#else
typedef intptr_t	int_val;
typedef uintptr_t	uint_val;
#endif

typedef enum {
	VAL_INT			= 0xFF,
	VAL_NULL		= 0,
	VAL_FLOAT		= 1,
	VAL_BOOL		= 2,
	VAL_STRING		= 3,
	VAL_OBJECT		= 4,
	VAL_ARRAY		= 5,
	VAL_FUNCTION	= 6,
	VAL_ABSTRACT	= 7,
	VAL_PRIMITIVE	= 6 | 8,
	VAL_32_BITS		= 0xFFFFFFFF
} val_type;

struct _value {
	val_type t;
};

struct _field;
struct _objtable;
struct _bufffer;
struct _vkind;
typedef struct _vkind *vkind;
typedef struct _value *value;
typedef struct _field *field;
typedef struct _objtable* objtable;
typedef struct _buffer *buffer;
typedef double tfloat;

typedef void (*finalizer)(value v);

typedef struct {
	val_type t;
	tfloat f;
} vfloat;

typedef struct {
	val_type t;
	objtable table;
} vobject;

typedef struct {
	val_type t;
	int_val nargs;
	void *addr;
	value env;
	void *module;
} vfunction;

typedef struct {
	val_type t;
	char c;
} vstring;

typedef struct {
	val_type t;
	value ptr;
} varray;

typedef struct {
	val_type t;
	vkind kind;
	void *data;
} vabstract;

#define val_tag(v)			(*(val_type*)v)
#define val_is_null(v)		(v == val_null)
#define val_is_int(v)		((((int_val)(v)) & 1) != 0)
#define val_is_bool(v)		(v == val_true || v == val_false)
#define val_is_number(v)	(val_is_int(v) || val_tag(v) == VAL_FLOAT)
#define val_is_float(v)		(!val_is_int(v) && val_tag(v) == VAL_FLOAT)
#define val_is_string(v)	(!val_is_int(v) && (val_tag(v)&7) == VAL_STRING)
#define val_is_function(v)	(!val_is_int(v) && (val_tag(v) == VAL_FUNCTION || val_tag(v) == VAL_PRIMITIVE))
#define val_is_object(v)	(!val_is_int(v) && val_tag(v) == VAL_OBJECT)
#define val_is_array(v)		(!val_is_int(v) && (val_tag(v)&7) == VAL_ARRAY)
#define val_is_abstract(v)  (!val_is_int(v) && val_tag(v) == VAL_ABSTRACT)
#define val_is_kind(v,t)	(val_is_abstract(v) && val_kind(v) == (t))
#define val_check_kind(v,t)	if( !val_is_kind(v,t) ) type_error();
#define val_check_function(f,n) if( !val_is_function(f) || (val_fun_nargs(f) != n && val_fun_nargs(f) != VAR_ARGS) ) type_error();
#define val_check(v,t)		if( !val_is_##t(v) ) type_error();
#define val_data(v)			((vabstract*)v)->data
#define val_kind(v)			((vabstract*)v)->kind

#define val_type(v)			(val_is_int(v) ? VAL_INT : (val_tag(v)&7))
#define val_int(v)			(((int)(int_val)(v)) >> 1)
#define val_float(v)		(CONV_FLOAT ((vfloat*)(v))->f)
#define val_bool(v)			(v == val_true)
#define val_number(v)		(val_is_int(v)?val_int(v):val_float(v))
#define val_string(v)		(&((vstring*)(v))->c)
#define val_to_field(v)		(field)(((int_val)(v)) >> 1)
#define val_strlen(v)		(val_tag(v) >> 3)
#define val_set_length(v,l) val_tag(v) = (val_tag(v)&7) | ((l) << 3)
#define val_set_size		val_set_length

#define val_array_size(v)	((int)(val_tag(v) >> 3))
#define val_array_ptr(v)	(&((varray*)(v))->ptr)
#define val_fun_nargs(v)	((int)((vfunction*)(v))->nargs)
#define alloc_int(v)		((value)(int_val)(((v) << 1) | 1))
#define alloc_bool(b)		((b)?val_true:val_false)

#define max_array_size		((1 << 29) - 1)
#define max_string_size		((1 << 29) - 1)
#define invalid_comparison	0xFF

#undef EXTERN
#undef EXPORT
#undef IMPORT
#ifdef _WIN32
#	define INLINE __inline
#	define EXPORT __declspec( dllexport )
#	define IMPORT __declspec( dllimport )
#else
#	define INLINE inline
#	define EXPORT
#	define IMPORT
#endif

#ifdef NEKO_SOURCES
#	define EXTERN EXPORT
#else
#	define EXTERN IMPORT
#endif

#ifdef __cplusplus
#	define C_FUNCTION_BEGIN extern "C" {
#	define C_FUNCTION_END	};
#else
#	define C_FUNCTION_BEGIN
#	define C_FUNCTION_END
#	ifndef true
#		define true 1
#		define false 0
		typedef int_val bool;
#	endif
#endif

#define type_error()		return NULL
#define failure(msg)		_neko_failure(alloc_string(msg),__FILE__,__LINE__)
#define bfailure(buf)		_neko_failure(buffer_to_string(b),__FILE__,__LINE__)

#ifndef CONV_FLOAT
#	define CONV_FLOAT
#endif

#define VAR_ARGS (-1)
#define DEFINE_PRIM_MULT(func) C_FUNCTION_BEGIN EXPORT void *func##__MULT() { return &func; } C_FUNCTION_END
#define DEFINE_PRIM(func,nargs) C_FUNCTION_BEGIN EXPORT void *func##__##nargs() { return &func; } C_FUNCTION_END
#define DEFINE_KIND(name) int_val __kind_##name = 0; vkind name = (vkind)&__kind_##name;
#define DEFINE_ENTRY_POINT(name) C_FUNCTION_BEGIN void name(); EXPORT void *__neko_entry_point() { return &name; } C_FUNCTION_END

#ifdef HEADER_IMPORTS
#	define DECLARE_PRIM(func,nargs) C_FUNCTION_BEGIN IMPORT void *func##__##nargs(); C_FUNCTION_END
#	define DECLARE_KIND(name) C_FUNCTION_BEGIN IMPORT extern vkind name; C_FUNCTION_END
#else
#	define DECLARE_PRIM(func,nargs) C_FUNCTION_BEGIN EXPORT void *func##__##nargs(); C_FUNCTION_END
#	define DECLARE_KIND(name) C_FUNCTION_BEGIN EXPORT extern vkind name; C_FUNCTION_END
#endif

#define alloc_float			neko_alloc_float
#define alloc_string		neko_alloc_string
#define alloc_empty_string	neko_alloc_empty_string
#define copy_string			neko_copy_string
#define val_this			neko_val_this
#define val_id				neko_val_id
#define val_field			neko_val_field
#define alloc_object		neko_alloc_object
#define alloc_field			neko_alloc_field
#define alloc_array			neko_alloc_array
#define val_call0			neko_val_call0
#define val_call1			neko_val_call1
#define val_call2			neko_val_call2
#define val_callN			neko_val_callN
#define val_ocall0			neko_val_ocall0
#define val_ocall1			neko_val_ocall1
#define val_ocallN			neko_val_ocallN
#define val_callEx			neko_val_callEx
#define	alloc_root			neko_alloc_root
#define free_root			neko_free_root
#define alloc				neko_alloc
#define alloc_private		neko_alloc_private
#define alloc_abstract		neko_alloc_abstract
#define alloc_function		neko_alloc_function
#define alloc_buffer		neko_alloc_buffer
#define buffer_append		neko_buffer_append
#define buffer_append_sub	neko_buffer_append_sub
#define buffer_to_string	neko_buffer_to_string
#define val_buffer			neko_val_buffer
#define val_compare			neko_val_compare
#define val_print			neko_val_print
#define val_gc				neko_val_gc
#define val_throw			neko_val_throw
#define val_iter_fields		neko_val_iter_fields
#define val_field_name		neko_val_field_name

C_FUNCTION_BEGIN

	EXTERN value val_null;
	EXTERN value val_true;
	EXTERN value val_false;

	EXTERN value alloc_float( tfloat t );

	EXTERN value alloc_string( const char *str );
	EXTERN value alloc_empty_string( uint_val size );
	EXTERN value copy_string( const char *str, uint_val size );

	EXTERN value val_this();
	EXTERN field val_id( const char *str );
	EXTERN value val_field( value o, field f );
	EXTERN value alloc_object( value o );
	EXTERN void alloc_field( value obj, field f, value v );
	EXTERN void val_iter_fields( value obj, void f( value v, field f, void * ), void *p );
	EXTERN value val_field_name( field f );

	EXTERN value alloc_array( uint_val n );
	EXTERN value alloc_abstract( vkind k, void *data );

	EXTERN value val_call0( value f );
	EXTERN value val_call1( value f, value arg );
	EXTERN value val_call2( value f, value arg1, value arg2 );
	EXTERN value val_call3( value f, value arg1, value arg2, value arg3 );
	EXTERN value val_callN( value f, value *args, int_val nargs );
	EXTERN value val_ocall0( value o, field f );
	EXTERN value val_ocall1( value o, field f, value arg );
	EXTERN value val_ocall2( value o, field f, value arg1, value arg2 );
	EXTERN value val_ocallN( value o, field f, value *args, int_val nargs );
	EXTERN value val_callEx( value this, value f, value *args, int_val nargs, value *exc );

	EXTERN value *alloc_root( uint_val nvals );
	EXTERN void free_root( value *r );
	EXTERN char *alloc( uint_val nbytes );
	EXTERN char *alloc_private( uint_val nbytes );
	EXTERN value alloc_function( void *c_prim, uint_val nargs, const char *name );

	EXTERN buffer alloc_buffer( const char *init );
	EXTERN void buffer_append( buffer b, const char *s );
	EXTERN void buffer_append_sub( buffer b, const char *s, int_val len );
	EXTERN value buffer_to_string( buffer b );
	EXTERN void val_buffer( buffer b, value v );

	EXTERN int_val val_compare( value a, value b );
	EXTERN void val_print( value s );
	EXTERN void val_gc( value v, finalizer f );
	EXTERN void val_throw( value v );

	EXTERN void _neko_failure( value msg, const char *file, int_val line );

C_FUNCTION_END

#endif
/* ************************************************************************ */
