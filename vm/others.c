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
#include <string.h>
#include <stdio.h>
#include "neko.h"
#include "objtable.h"
#include "vmcontext.h"

#define C(x,y)	((x << 8) | y)

extern _context *neko_fields_context;
extern field id_compare;
extern field id_string;

static INLINE int icmp( int a, int b ) {
	return (a == b)?0:((a < b)?-1:1);
}

static INLINE int fcmp( tfloat a, tfloat b ) {
	return (a == b)?0:((a < b)?-1:1);
}

static INLINE int scmp( const char *s1, int l1, const char *s2, int l2 ) {
	int r = memcmp(s1,s2,(l1 < l2)?l1:l2); 
	return r?r:icmp(l1,l2);
}

EXTERN int val_compare( value a, value b ) {
	char tmp_buf[32];
	switch( C(val_type(a),val_type(b)) ) {
	case C(VAL_INT,VAL_INT):
		return icmp(val_int(a),val_int(b));
	case C(VAL_INT,VAL_FLOAT):
		return fcmp(val_int(a),val_float(b));
	case C(VAL_INT,VAL_STRING): 
		return scmp(tmp_buf,sprintf(tmp_buf,"%d",val_int(a)),val_string(b),val_strlen(b));
	case C(VAL_FLOAT,VAL_INT):
		return fcmp(val_float(a),val_int(b));
	case C(VAL_FLOAT,VAL_FLOAT):
		return fcmp(val_float(a),val_float(b));
	case C(VAL_FLOAT,VAL_STRING):
		return scmp(tmp_buf,sprintf(tmp_buf,"%.10g",val_float(a)),val_string(b),val_strlen(b));
	case C(VAL_STRING,VAL_INT):
		return scmp(val_string(a),val_strlen(a),tmp_buf,sprintf(tmp_buf,"%d",val_int(b)));
	case C(VAL_STRING,VAL_FLOAT):
		return scmp(val_string(a),val_strlen(a),tmp_buf,sprintf(tmp_buf,"%.10g",val_float(b)));
	case C(VAL_STRING,VAL_BOOL):
		return scmp(val_string(a),val_strlen(a),val_bool(b)?"true":"false",val_bool(b)?4:5);
	case C(VAL_BOOL,VAL_STRING):
		return scmp(val_bool(a)?"true":"false",val_bool(a)?4:5,val_string(b),val_strlen(b));
	case C(VAL_STRING,VAL_STRING):
		return scmp(val_string(a),val_strlen(a),val_string(b),val_strlen(b));
	case C(VAL_OBJECT,VAL_OBJECT):
		if( a == b )
			return 0;
		a = val_ocall1(a,id_compare,b);
		if( val_is_int(a) )
			return val_int(a);
		return invalid_comparison;
	default:
		if( a == b )
			return 0;
		return invalid_comparison;
	}
}

typedef struct _stringitem {
	char *str;
	int len;
	struct _stringitem *next;
} * stringitem;

struct _buffer {
	int totlen;
	stringitem data;
};

EXTERN buffer alloc_buffer( const char *init ) {
	buffer b = (buffer)alloc(sizeof(struct _buffer));
	b->totlen = 0;
	b->data = NULL;
	if( init )
		buffer_append(b,init);
	return b;
}

EXTERN void buffer_append_sub( buffer b, const char *s, int len ) {	
	stringitem it;
	if( s == NULL || len <= 0 )
		return;
	b->totlen += len;
	it = (stringitem)alloc(sizeof(struct _stringitem));
	it->str = alloc_private(len+1);
	memcpy(it->str,s,len);
	it->str[len] = 0;
	it->len = len;
	it->next = b->data;
	b->data = it;
}

EXTERN void buffer_append( buffer b, const char *s ) {
	if( s == NULL )
		return;
	buffer_append_sub(b,s,strlen(s));
}

EXTERN value buffer_to_string( buffer b ) {
	value v = alloc_empty_string(b->totlen);
	stringitem it = b->data;
	char *s = (char*)val_string(v) + b->totlen;
	while( it != NULL ) {
		stringitem tmp;
		s -= it->len;
		memcpy(s,it->str,it->len);
		tmp = it->next;
		it = tmp;
	}
	return v;
}

EXTERN void val_buffer( buffer b, value v ) {
	char buf[32];
	int i, l;
	switch( val_type(v) ) {
	case VAL_INT:
		buffer_append_sub(b,buf,sprintf(buf,"%d",val_int(v)));
		break;
	case VAL_STRING:
		buffer_append_sub(b,val_string(v),val_strlen(v));
		break;
	case VAL_FLOAT:
		buffer_append_sub(b,buf,sprintf(buf,"%.10g",val_float(v)));
		break;
	case VAL_NULL:
		buffer_append_sub(b,"null",4);
		break;
	case VAL_BOOL:
		if( val_bool(v) )
			buffer_append_sub(b,"true",4);
		else
			buffer_append_sub(b,"false",5);
		break;
	case VAL_FUNCTION:
		buffer_append_sub(b,buf,sprintf(buf,"#function:%d",val_fun_nargs(v)));
		break;	
	case VAL_OBJECT:
		v = val_ocall0(v,id_string);
		if( val_is_string(v) )
			buffer_append_sub(b,val_string(v),val_strlen(v));
		else
			buffer_append_sub(b,"#object",7);
		break;
	case VAL_ARRAY:
		buffer_append_sub(b,"[",1);
		l = val_array_size(v) - 1;
		for(i=0;i<l;i++) {
			val_buffer(b,val_array_ptr(v)[i]);
			buffer_append_sub(b,",",1);
		}
		if( l >= 0 )
			val_buffer(b,val_array_ptr(v)[l]);
		buffer_append_sub(b,"]",1);
		break;
	case VAL_ABSTRACT:
		buffer_append_sub(b,"#abstract",9);
		break;
	default:
		buffer_append_sub(b,"#unknown",8);
		break;
	}
}

// theses two 'append' function belongs here because we don't want 
// them to be inlined in interp.c by GCC since this will break 
// register allocation

value append_int( neko_vm *vm, value str, int x, bool way ) {
	int len, len2;
	value v;
	len = val_strlen(str);
	len2 = sprintf(vm->tmp,"%d",x);
	v = alloc_empty_string(len+len2);
	if( way ) {
		memcpy((char*)val_string(v),val_string(str),len);
		memcpy((char*)val_string(v)+len,vm->tmp,len2+1);
	} else {
		memcpy((char*)val_string(v),vm->tmp,len2);
		memcpy((char*)val_string(v)+len2,val_string(str),len+1);
	}
	return v;
}

value append_strings( value s1, value s2 ) {
	int len1 = val_strlen(s1);
	int len2 = val_strlen(s2);
	value v = alloc_empty_string(len1+len2);
	memcpy((char*)val_string(v),val_string(s1),len1);
	memcpy((char*)val_string(v)+len1,val_string(s2),len2+1);
	return v;
}

int neko_stack_expand( int *sp, int *csp, neko_vm *vm ) {
	int i;
	int size = (((int)vm->spmax - (int)vm->spmin) / sizeof(int)) << 1;
	int *nsp;
	if( size > MAX_STACK_SIZE )
		return 0;
	nsp = (int*)alloc(size * sizeof(int));
	
	// csp size
	i = ((int)(csp + 1) - (int)vm->spmin) / sizeof(int);
	memcpy(nsp,vm->spmin,sizeof(int) * i);
	vm->csp = nsp + i - 1;
	
	// sp size
	i = ((int)vm->spmax - (int)sp) / sizeof(int);
	memcpy(nsp+size-i,sp,sizeof(int) * i);
	vm->sp = nsp + size - i;
	vm->spmin = nsp;
	vm->spmax = nsp + size;
	return 1;
}

EXTERN field val_id( const char *name ) {
	objtable *data;
	value *fdata;
	field f;
	value acc = alloc_int(0);
	const char *oname = name;
	while( *name ) {
		acc = alloc_int(223 * val_int(acc) + *((unsigned char*)name));
		name++;
	}
	f = (field)val_int(acc);
	data = (objtable*)context_get(neko_fields_context);
	if( data == NULL ) {
		data = (objtable*)alloc_root(1);
		*data = otable_empty();
		context_set(neko_fields_context,data);
	}
	fdata = otable_find(*data,f);
	if( fdata != NULL ) {
		if( scmp(val_string(*fdata),val_strlen(*fdata),oname,name - oname) != 0 ) {
			buffer b = alloc_buffer("Field conflict between ");
			val_buffer(b,*fdata);
			buffer_append(b," and ");
			buffer_append(b,oname);
			bfailure(b);
		}
	} else
		otable_replace(*data,f,copy_string(oname,name - oname));
	return f;
}

EXTERN value val_field_name( field id ) {
	objtable *data = (objtable*)context_get(neko_fields_context);
	value *fdata;
	if( data == NULL )
		return val_null;
	fdata = otable_find(*data,id);
	if( fdata == NULL )
		return val_null;
	return *fdata;
}

EXTERN void neko_clean_thread() {
	void *f = context_get(neko_fields_context);
	if( f )
		free_root(f);
	context_set(neko_vm_context,NULL);
}

EXTERN value val_field( value o, field id ) {
	value *f;
	f = otable_find(((vobject*)o)->table,id);
	if( f == NULL )
		return val_null;
	return *f;
}

EXTERN void val_iter_fields( value o, void f( value , field, void * ) , void *p ) {
	otable_iter( ((vobject*)o)->table, f, p );
}

EXTERN void val_print( value v ) {
	if( !val_is_string(v) ) {
		buffer b = alloc_buffer(NULL);
		val_buffer(b,v);
		v = buffer_to_string(b);
	}
	NEKO_VM()->print( val_string(v), val_strlen(v) );
}

EXTERN void val_throw( value v ) {
	neko_vm *vm = NEKO_VM();
	vm->this = v;
	longjmp(vm->start,1);
}

static value failure_to_string() {
	value o = val_this();
	buffer b = alloc_buffer(NULL);
	val_check(o,object);
	val_buffer(b,val_field(o,val_id("file")));
	buffer_append(b,"(");
	val_buffer(b,val_field(o,val_id("line")));
	buffer_append(b,") : ");
	val_buffer(b,val_field(o,val_id("msg")));
	return buffer_to_string(b);
}

EXTERN void _neko_failure( value msg, const char *file, int line ) {
	char *fname = strrchr(file,'/');
	char *fname2 = strrchr(file,'\\');
	value o = alloc_object(NULL);
	if( fname2 > fname )
		fname = fname2;
	alloc_field(o,val_id("msg"),msg);
	alloc_field(o,val_id("file"),alloc_string(fname?(fname+1):file));
	alloc_field(o,val_id("line"),alloc_int(line));
	alloc_field(o,id_string,alloc_function(failure_to_string,0,"failure_to_string"));
	val_throw(o);
}

/* ************************************************************************ */
