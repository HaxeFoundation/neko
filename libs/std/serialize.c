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
#include <string.h>
#include <neko.h>
#include <load.h>

#define BUF_SIZE	4096

typedef struct strlist {
	unsigned char *str;
	int slen;
	struct strlist *next;
} strlist;

typedef struct odatalist {
	value data;
	struct odatalist *next;
} odatalist;

typedef struct {
	odatalist *refs;
	strlist *olds;
	unsigned char *cur;
	bool error;
	int size;
	int pos;
	int totlen;
} sbuffer;

extern field id_module;
extern field id_loadmodule;

static void buffer_alloc( sbuffer *b, int size ) {
	strlist *str = (strlist*)alloc(sizeof(strlist));
	str->str = b->cur;
	str->slen = b->pos;
	str->next = b->olds;
	b->olds = str;
	b->totlen += b->pos;
	b->pos = 0;
	b->size = size;
	b->cur = (unsigned char*)alloc_private(size);
}

static void write_char( sbuffer *b, char c ) {
	if( b->pos == b->size )
		buffer_alloc(b,BUF_SIZE);
	b->cur[b->pos++] = c;
}

static int read_char( sbuffer *b ) {
	if( b->pos >= b->size )
		return -1;
	return b->cur[b->pos++];
}

static int peek_char( sbuffer *b ) {
	if( b->pos >= b->size )
		return -1;
	return b->cur[b->pos];
}

static void write_str( sbuffer *b, int len, const void *s ) {
	int left = b->size - b->pos;
	if( left == 0 ) {
		buffer_alloc(b,BUF_SIZE);
		left = b->size - b->pos;
	}
	if( left >= len ) {
		memcpy(b->cur + b->pos,s,len);
		b->pos += len;
	} else {
		memcpy(b->cur + b->pos,s,left);
		b->pos += left;
		write_str(b,len - left, (char*)s + left);
	}
}

static void read_str( sbuffer *b, int len, void *s ) {
	if( b->pos + len > b->size ) {
		b->error = true;
		return;
	}
	memcpy(s,b->cur + b->pos, len);
	b->pos += len;
}

static void write_int( sbuffer *b, int n ) {
	write_str(b,4,&n);
}

static int read_int( sbuffer *b ) {
	int n;
	if( b->pos + 4 > b->size ) {
		b->error = true;
		return 0;
	}
	memcpy(&n,b->cur + b->pos,4);
	b->pos += 4;
	return n;
}

static void add_ref( sbuffer *b, value o ) {
	odatalist *d = (odatalist*)alloc(sizeof(odatalist));
	d->data = o;
	d->next = b->refs;
	b->refs = d;
}

static bool write_ref( sbuffer *b, value o ) {
	int i = 0;
	odatalist *d = b->refs;
	while( d != NULL ) {
		if( d->data == o ) {
			write_char(b,'r');
			write_int(b,i);
			return true;
		}
		i++;
		d = d->next;
	}
	add_ref(b,o);
	return false;
}

static void serialize_fields_rec( value data, field id, void *b );

void serialize_rec( sbuffer *b, value o ) {
	switch( val_type(o) ) {
	case VAL_NULL:
		write_char(b,'N');
		break;
	case VAL_BOOL:
		if( val_bool(o) )
			write_char(b,'T');
		else
			write_char(b,'F');
		break;
	case VAL_INT:
		write_char(b,'i');
		write_int(b,val_int(o));
		break;
	case VAL_FLOAT:
		write_char(b,'f');
		write_str(b,8,&val_float(o));
		break;
	case VAL_STRING:
		if( !write_ref(b,o) ) {
			write_char(b,'s');
			write_int(b,val_strlen(o));
			write_str(b,val_strlen(o),val_string(o));
		}
		break;
	case VAL_OBJECT:
		if( !write_ref(b,o) ) {
			write_char(b,'o');
			val_iter_fields(o,serialize_fields_rec,b);
			write_int(b,0);
		}
		break;
	case VAL_ARRAY:
		if( !write_ref(b,o) ) {
			int i;
			int n = val_array_size(o);
			write_char(b,'a');
			write_int(b,n);
			for(i=0;i<n;i++)
				serialize_rec(b,val_array_ptr(o)[i]);
		}
		break;
	case VAL_FUNCTION:
		if( !write_ref(b,o) ) {
			neko_module *m;
			if( val_tag(o) == VAL_PRIMITIVE )
				failure("Cannot Serialize Primitive");
			write_char(b,'L');
			m = (neko_module*)((vfunction*)o)->module;
			serialize_rec(b,m->name);
			write_int(b,(int)((int_val*)((vfunction*)o)->addr - m->code));
			write_int(b,((vfunction*)o)->nargs);
			serialize_rec(b,((vfunction*)o)->env);
		}
		break;
	case VAL_ABSTRACT:
		if( val_is_kind(o,k_int32) ) {
			write_char(b,'I');
			write_int(b,val_int32(o));
			break;
		}
	default:
		failure("Cannot Serialize Abstract");
		break;
	}
}

static void serialize_fields_rec( value data, field id, void *b ) {
	write_int(b,(int)id);
	serialize_rec(b,data);
}

static value serialize( value o ) {
	sbuffer b;
	value v;
	char *s;
	strlist *l;
	b.olds = NULL;
	b.refs = NULL; 
	b.cur = (unsigned char*)alloc_private(BUF_SIZE);
	b.size = BUF_SIZE;
	b.pos = 0;
	b.totlen = 0;
	b.error = false;
	serialize_rec(&b,o);
	v = alloc_empty_string(b.pos + b.totlen);
	s = (char*)val_string(v);
	s += b.totlen;
	l = b.olds;
	memcpy(s,b.cur,b.pos);
	while( l != NULL ) {
		s -= l->slen;
		memcpy(s,l->str,l->slen);
		l = l->next;
	}
	return v;
}

static value unserialize_rec( sbuffer *b, value loader ) {
	switch( read_char(b) ) {
	case 'N':
		return val_null;
	case 'T':
		return val_true;
	case 'F':
		return val_false;
	case 'i':
		return alloc_int(read_int(b));
	case 'I':
		return alloc_int32(read_int(b));
	case 'f':
		{
			double d;
			read_str(b,8,&d);
			return alloc_float(d);
		}
	case 's':
		{
			int l = read_int(b);
			value v;
			if( l < 0 || l >= 0x1000000 ) {
				b->error = true;
				return val_null;
			}
			v = alloc_empty_string(l);
			add_ref(b,v);
			read_str(b,l,(char*)val_string(v));
			return v;
		}
	case 'o':
		{
			int f;
			value o = alloc_object(NULL);
			add_ref(b,o);
			while( (f = read_int(b)) != 0 ) {
				value fval = unserialize_rec(b,loader);
				alloc_field(o,(field)f,fval);
			}
			return o;
		}
	case 'r':
		{
			int n = read_int(b);
			odatalist *d = b->refs;
			while( n > 0 && d != NULL ) {
				d = d->next;
				n--;
			}
			if( d == NULL ) {
				b->error = true;
				return val_null;
			}
			return d->data;
		}
	case 'a':
		{
			int i;
			int n = read_int(b);
			value o;
			value *t;
			if( n < 0 || n >= 0x100000 ) {
				b->error = true;
				return val_null;
			}
			o = alloc_array(n);
			t = val_array_ptr(o);
			add_ref(b,o);
			for(i=0;i<n;i++)
				t[i] = unserialize_rec(b,loader);
			return o;

		}
	case 'L':
		{
			vfunction *f = (vfunction*)alloc_function((void*)1,0,NULL);
			value mname; 
			int pos;
			int nargs;
			value env;
			add_ref(b,(value)f);
			mname = unserialize_rec(b,loader);
			pos = read_int(b);
			nargs = read_int(b);
			env = unserialize_rec(b,loader);
			if( !val_is_string(mname) || !val_is_array(env) ) {
				b->error = true;
				return val_null;
			}
			{
				value exp = val_ocall2(loader,id_loadmodule,mname,loader);
				value mval;
				unsigned int i;
				int_val *mpos;
				neko_module *m;
				if( !val_is_object(exp) ) {
					buffer b = alloc_buffer("module ");
					val_buffer(b,mname);
					buffer_append(b," is not an object");
					bfailure(b);
				}
				mval = val_field(exp,id_module);
				if( !val_is_kind(mval,k_module) ) {
					buffer b = alloc_buffer("module ");
					val_buffer(b,mname);
					buffer_append(b," have invalid type");
					bfailure(b);
				}
				m = (neko_module*)val_data(mval);
				mpos = m->code + pos;
				for(i=0;i<m->nglobals;i++) {
					vfunction *g = (vfunction*)m->globals[i];
					if( val_is_function(g) && g->addr == mpos && g->module == m && g->nargs == nargs ) {
						f->t = VAL_FUNCTION;
						f->env = env;
						f->addr = mpos;
						f->nargs = nargs;
						f->module = m;
						return (value)f;
					}
				}
				{
					buffer b = alloc_buffer("module ");
					val_buffer(b,mname);
					buffer_append(b," have been modified");
					bfailure(b);
				}
			}
			return val_null;
		}
	default:
		b->error = true;
		return val_null;
	}
}

static value unserialize( value s, value loader ) {
	value v;
	sbuffer b;
	val_check(s,string);
	b.cur = (unsigned char*)val_string(s);
	b.pos = 0;
	b.error = false;
	b.olds = NULL;
	b.refs = NULL;
	b.size = val_strlen(s);
	b.totlen = 0;
	v = unserialize_rec(&b,loader);
	if( b.error )
		failure("Invalid serialized data");
	return v;
}

DEFINE_PRIM(serialize,1);
DEFINE_PRIM(unserialize,2);

/* ************************************************************************ */
