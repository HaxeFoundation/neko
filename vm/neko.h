/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#ifndef _NEKO_H
#define _NEKO_H

#ifndef NULL
#	define NULL			0
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
	VAL_PRIMITIVE	= 7,
	VAL_32_BITS		= 0xFFFFFFFF
} val_type;

struct _value {
	val_type t;
};

struct _otype;
struct _field;
struct _objtable;
struct _bufffer;
typedef struct _value* value;
typedef struct _otype *otype;
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
	otype ot;
	value data;
	objtable table;
} vobject;

typedef struct {
	val_type t;
	int nargs;
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

#define t_class				((otype)0x00000001)
#define val_tag(v)			(*(val_type*)v)
#define val_is_null(v)		(v == val_null)
#define val_is_int(v)		((((int)(v)) & 1) != 0)
#define val_is_bool(v)		(v == val_true || v == val_false)
#define val_is_number(v)	(val_is_int(v) || val_tag(v) == VAL_FLOAT)
#define val_is_float(v)		(!val_is_int(v) && val_tag(v) == VAL_FLOAT)
#define val_is_string(v)	(!val_is_int(v) && (val_tag(v)&7) == VAL_STRING)
#define val_is_function(v)	(!val_is_int(v) && (val_tag(v) == VAL_FUNCTION || val_tag(v) == VAL_PRIMITIVE))
#define val_is_object(v)	(!val_is_int(v) && val_tag(v) == VAL_OBJECT)
#define val_is_array(v)		(!val_is_int(v) && (val_tag(v)&7) == VAL_ARRAY)
#define val_is_obj(v,t)		(val_is_object(v) && val_otype(v) == t)
#define val_check_obj(v,t)	{ if( !val_is_obj(v,t) ) return val_null; }

#define val_type(v)			(val_is_int(v) ? VAL_INT : (val_tag(v)&7))
#define val_int(v)			(((int)(v)) >> 1)
#define val_float(v)		((vfloat*)(v))->f
#define val_bool(v)			(v == val_true)
#define val_number(v)		(val_is_int(v)?val_int(v): CONV_FLOAT(val_float(v)) )
#define val_string(v)		(&((vstring*)(v))->c)
#define val_strlen(v)		(val_tag(v) >> 3)

#define val_array_size(v)	(val_tag(v) >> 3)
#define val_array_ptr(v)	(&((varray*)(v))->ptr)
#define val_fun_nargs(v)	((vfunction*)(v))->nargs
#define val_otype(v)		((vobject*)(v))->ot
#define val_odata(v)		((vobject*)(v))->data
#define alloc_int(v)		((value)(((v) << 1) | 1))
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

#ifdef MTSVM_SOURCE
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
		typedef int bool;
#	endif
#endif

#ifndef CONV_FLOAT
#	define CONV_FLOAT
#endif

#define VAR_ARGS (-1)
#define DEFINE_PRIM_MULT(func) C_FUNCTION_BEGIN EXPORT void *func##__MULT() { return &func; } C_FUNCTION_END
#define DEFINE_PRIM(func,nargs) C_FUNCTION_BEGIN EXPORT void *func##__##nargs() { return &func; } C_FUNCTION_END
#define DEFINE_CLASS(dll,name) extern value dll##_##name(); otype t_##name = (otype)dll##_##name; DEFINE_PRIM(dll##_##name,0);

#ifdef CLASS_PRIM_IMPORTS
#	define DECLARE_PRIM(func,nargs) C_FUNCTION_BEGIN IMPORT void *func##__##nargs(); C_FUNCTION_END
#	define DECLARE_CLASS(name) C_FUNCTION_BEGIN IMPORT extern otype t_##name; C_FUNCTION_END
#else
#	define DECLARE_PRIM(func,nargs) C_FUNCTION_BEGIN EXPORT void *func##__##nargs(); C_FUNCTION_END
#	define DECLARE_CLASS(name) C_FUNCTION_BEGIN EXPORT extern otype t_##name; C_FUNCTION_END
#endif

typedef value (*PRIM0)();
typedef value (*PRIM1)( value v );
typedef value (*PRIM2)( value v1, value v2 );

#define CALL_PRIM(func) ((PRIM0)func##__0())()
#define CALL_PRIM1(func,v) ((PRIM1)func##__1())(v)
#define CALL_PRIM2(func,v1,v2) ((PRIM2)func##__2())(v1,v2)

#define alloc_float			neko_alloc_float
#define alloc_string		neko_alloc_string
#define alloc_empty_string	neko_alloc_empty_string
#define copy_string			neko_copy_string
#define val_this			neko_val_this
#define val_id				neko_val_id
#define val_field			neko_val_field
#define copy_object			neko_copy_object
#define alloc_object		neko_alloc_object
#define alloc_class			neko_alloc_class
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

C_FUNCTION_BEGIN

	EXTERN value val_null;
	EXTERN value val_true;
	EXTERN value val_false;

	EXTERN value alloc_float( tfloat t );

	EXTERN value alloc_string( const char *str );
	EXTERN value alloc_empty_string( unsigned int size );
	EXTERN value copy_string( const char *str, unsigned int size );

	EXTERN value val_this();
	EXTERN field val_id( const char *str );
	EXTERN value val_field( value o, field f );
	EXTERN value copy_object( value o );
	EXTERN value alloc_object( otype *t );
	EXTERN value alloc_class( otype *t );
	EXTERN void alloc_field( value obj, field f, value v );

	EXTERN value alloc_array( unsigned int n );

	EXTERN value val_call0( value f );
	EXTERN value val_call1( value f, value arg );
	EXTERN value val_call2( value f, value arg1, value arg2 );
	EXTERN value val_callN( value f, value *args, int nargs );
	EXTERN value val_ocall0( value o, field f );
	EXTERN value val_ocall1( value o, field f, value arg );
	EXTERN value val_ocallN( value o, field f, value *args, int nargs );
	EXTERN value val_callEx( value this, value f, value *args, int nargs, value *exc );

	EXTERN value *alloc_root( unsigned int nvals );
	EXTERN void free_root( value *r );
	EXTERN char *alloc( unsigned int nbytes );
	EXTERN char *alloc_abstract( unsigned int nbytes );
	EXTERN value alloc_function( void *c_prim, unsigned int nargs );

	EXTERN buffer alloc_buffer( const char *init );
	EXTERN void buffer_append( buffer b, const char *s );
	EXTERN void buffer_append_sub( buffer b, const char *s, int len );
	EXTERN value buffer_to_string( buffer b );
	EXTERN void val_buffer( buffer b, value v );

	EXTERN int val_compare( value a, value b );
	EXTERN void val_print( value s );
	EXTERN void val_gc( value v, finalizer f );
	EXTERN void val_throw( value v );

C_FUNCTION_END

#define Constr(o,t,nargs) { field f__new_##nargs = val_id("new"); alloc_field(o,f__new_##nargs,alloc_function(t##_new##nargs,nargs) ); }
#define Method(o,t,name,nargs) { field f__##name = val_id(#name); alloc_field(o,f__##name,alloc_function(t##_##name,nargs) ); }
#define MethodMult(o,t,name) { field f__##name = val_id(#name); alloc_field(o,f__##name,alloc_function(t##_##name,VAR_ARGS) ); }
#define Property(o,t,name)	Method(o,t,get_##name,0); Method(o,t,set_##name,1)

#endif/* ************************************************************************ */
