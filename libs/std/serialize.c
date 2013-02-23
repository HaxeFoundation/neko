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
#include <string.h>
#include <neko_mod.h>

#define BUF_SIZE	4096
#define ERROR()		val_throw(alloc_string("Invalid serialized data"))

typedef struct strlist {
	unsigned char *str;
	int slen;
	struct strlist *next;
} strlist;

typedef struct odatalist {
	int k;
	value data;
	struct odatalist *left;
	struct odatalist *right;
} odatalist;

typedef struct {
	odatalist *refs;
	int nrefs;
	value *trefs;
	int tsize;
	strlist *olds;
	unsigned char *cur;
	int size;
	int pos;
	int totlen;
	int nrec;
} sbuffer;

extern field id_module;
extern field id_loadmodule;
extern field id_loadprim;
extern field id_serialize;
extern field id_unserialize;

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
	if( b->pos + len > b->size )
		ERROR();
	memcpy(s,b->cur + b->pos, len);
	b->pos += len;
}

static void write_int( sbuffer *b, int n ) {
	write_str(b,4,&n);
}

static int read_int( sbuffer *b ) {
	int n;
	if( b->pos + 4 > b->size )
		ERROR();
	memcpy(&n,b->cur + b->pos,4);
	b->pos += 4;
	return n;
}

static void lookup_serialize_field( value data, field id, void *v ) {
	if( id == id_serialize )
		*(value*)v = data;
}

static bool write_ref( sbuffer *b, value o, value *serialize ) {
	odatalist *d = b->refs, *prev = NULL;
	while( d != NULL ) {
		if( d->data < o ) {
			prev = d;
			d = d->left;
		} else if( d->data == o ) {
			write_char(b,'r');
			write_int(b,b->nrefs - 1 - d->k);
			return true;
		} else {
			prev = d;
			d = d->right;
		}
	}
	if( serialize != NULL ) {
		*serialize = NULL;
		val_iter_fields(o,lookup_serialize_field,serialize);
		if( *serialize != NULL )
			return false;
	}
	d = (odatalist*)alloc(sizeof(odatalist));
	d->data = o;
	d->k = b->nrefs++;
	d->left = NULL;
	d->right = NULL;
	if( prev == NULL )
		b->refs = d;
	else if( prev->data < o )
		prev->left = d;
	else
		prev->right = d;
	return false;
}

static void add_ref( sbuffer *b, value v ) {
	if( b->nrefs == b->tsize ) {
		int nsize = b->tsize?(b->tsize*2):16;
		value *ntrefs = (value*)alloc(sizeof(value) * nsize);
		memcpy(ntrefs,b->trefs,b->tsize * sizeof(value));
		b->trefs = ntrefs;
		b->tsize = nsize;
	}
	b->trefs[b->nrefs++] = v;
}

static void serialize_fields_rec( value data, field id, void *b );

void serialize_rec( sbuffer *b, value o ) {
	b->nrec++;
	if( b->nrec > 350 )
		failure("Serialization stack overflow");
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
		write_str(b,sizeof(tfloat),&val_float(o));
		break;
	case VAL_STRING:
		if( !write_ref(b,o,NULL) ) {
			write_char(b,'s');
			write_int(b,val_strlen(o));
			write_str(b,val_strlen(o),val_string(o));
		}
		break;
	case VAL_OBJECT:
		{
			value s;
			if( !write_ref(b,o,&s) ) {
				if( s != NULL ) {
					// reference was not written
					if( !val_is_function(s) || (val_fun_nargs(s) != 0 && val_fun_nargs(s) != VAR_ARGS) )
						failure("Invalid __serialize method");
					write_char(b,'x');
					serialize_rec(b,((neko_module*)((vfunction*)s)->module)->name);
					serialize_rec(b,val_ocall0(o,id_serialize));
					// put reference back
					write_ref(b,o,NULL);
					break;
				}
				write_char(b,'o');
				val_iter_fields(o,serialize_fields_rec,b);
				write_int(b,0);
				o = (value)((vobject*)o)->proto;
				if( o == NULL )
					write_char(b,'z');
				else {
					write_char(b,'p');
					serialize_rec(b,o);
				}
			}
		}
		break;
	case VAL_ARRAY:
		if( !write_ref(b,o,NULL) ) {
			int i;
			int n = val_array_size(o);
			write_char(b,'a');
			write_int(b,n);
			for(i=0;i<n;i++)
				serialize_rec(b,val_array_ptr(o)[i]);
		}
		break;
	case VAL_FUNCTION:
		if( !write_ref(b,o,NULL) ) {
			neko_module *m;
			if( val_tag(o) == VAL_PRIMITIVE ) {
				// assume that alloc_array(0) return a constant array ptr
				// we don't want to access custom memory (maybe not a ptr)
				if( ((vfunction*)o)->env != alloc_array(0) )
					failure("Cannot Serialize Primitive with environment");
				write_char(b,'p');
				write_int(b,((vfunction*)o)->nargs);
				serialize_rec(b,((vfunction*)o)->module);
				break;
			}
			if( val_tag(o) == VAL_JITFUN )
				failure("Cannot Serialize JIT method");
			write_char(b,'L');
			m = (neko_module*)((vfunction*)o)->module;
			serialize_rec(b,m->name);
			write_int(b,(int)((int_val*)((vfunction*)o)->addr - m->code));
			write_int(b,((vfunction*)o)->nargs);
			serialize_rec(b,((vfunction*)o)->env);
		}
		break;
	case VAL_INT32:
		write_char(b,'I');
		write_int(b,val_int32(o));
		break;
	case VAL_ABSTRACT:
		if( val_is_kind(o,k_hash) ) {
			int i;
			vhash *h = val_hdata(o);
			write_char(b,'h');
			write_int(b,h->ncells);
			write_int(b,h->nitems);
			for(i=0;i<h->ncells;i++) {
				hcell *c = h->cells[i];
				while( c != NULL ) {
					write_int(b,c->hkey);
					serialize_rec(b,c->key);
					serialize_rec(b,c->val);
					c = c->next;
				}
			}
			break;
		}
	default:
		failure("Cannot Serialize Abstract");
		break;
	}
	b->nrec--;
}

static void serialize_fields_rec( value data, field id, void *b ) {
	write_int(b,(int)id);
	serialize_rec(b,data);
}

/**
	<doc>
	<h1>Serialize</h1>
	<p>
	Serialization can be used in order to store permanantly some runtime value.
	Serialization of all values is possible, except Abstracts, with the special
	cases of ['int32] and ['hash] which are handled as specific cases.
	</p>
	<p>
	Serialization of bytecode function is possible, but will result in a runtime
	exception when deserializing if the function offset in the bytecode has changed.
	</p>
	<p>
	You can define the __serialize method of an object. When this method is found when
	serializing the object, it is called with no arguments and its return value will be
	serialized. The name of the module the method is declared in will also be serialized.
	When unserializing, the module is loaded and its __unserialize exported function is
	called with the value that was returned by __serialize.
	</p>
	</doc>
**/

/**
	serialize : any -> string
	<doc>Serialize any value recursively</doc>
**/
static value serialize( value o ) {
	sbuffer b;
	value v;
	char *s;
	strlist *l;
	b.olds = NULL;
	b.refs = NULL;
	b.nrefs = 0;
	b.cur = (unsigned char*)alloc_private(BUF_SIZE);
	b.size = BUF_SIZE;
	b.pos = 0;
	b.totlen = 0;
	b.nrec = 0;
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
			tfloat d;
			read_str(b,sizeof(tfloat),&d);
			return alloc_float(d);
		}
	case 's':
		{
			int l = read_int(b);
			value v;
			if( l < 0 || l > max_string_size )
				ERROR();
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
			switch( read_char(b) ) {
			case 'p':
				{
					value v = unserialize_rec(b,loader);
					if( !val_is_object(v) )
						ERROR();
					((vobject*)o)->proto = (vobject*)v;
				}
				break;
			case 'z':
				break;
			default:
				ERROR();
			}
			return o;
		}
	case 'r':
		{
			int n = read_int(b);
			if( n < 0 || n >= b->nrefs )
				ERROR();
			return b->trefs[b->nrefs - n - 1];
		}
	case 'a':
		{
			int i;
			int n = read_int(b);
			value o;
			value *t;
			if( n < 0 || n > max_array_size )
				ERROR();
			o = alloc_array(n);
			t = val_array_ptr(o);
			add_ref(b,o);
			for(i=0;i<n;i++)
				t[i] = unserialize_rec(b,loader);
			return o;

		}
	case 'p':
		{
			int nargs = read_int(b);
			vfunction *f = (vfunction*)alloc_function((void*)1,nargs,NULL);
			vfunction *f2;
			value name;
			add_ref(b,(value)f);
			name = unserialize_rec(b,loader);
			f2 = (vfunction*)val_ocall2(loader,id_loadprim,name,alloc_int(nargs));
			if( !val_is_function(f2) || val_fun_nargs(f2) != nargs )
				failure("Loader returned not-a-function");
			f->t = f2->t;
			f->addr = f2->addr;
			f->module = f2->module;
			return (value)f;
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
			if( !val_is_array(env) )
				ERROR();
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
				if( !val_is_kind(mval,neko_kind_module) ) {
					buffer b = alloc_buffer("module ");
					val_buffer(b,mname);
					buffer_append(b," has invalid type");
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
					buffer_append(b," has been modified");
					bfailure(b);
				}
			}
			return val_null;
		}
	case 'x':
		{
			value mname = unserialize_rec(b,loader);
			value data = unserialize_rec(b,loader);
			value exports = val_ocall2(loader,id_loadmodule,mname,loader);
			value s;
			if( !val_is_object(exports) ) {
				buffer b = alloc_buffer("module ");
				val_buffer(b,mname);
				buffer_append(b," is not an object");
				bfailure(b);
			}
			s = val_field(exports,id_unserialize);
			if( !val_is_function(s) || (val_fun_nargs(s) != 1 && val_fun_nargs(s) != VAR_ARGS) ) {
				buffer b = alloc_buffer("module ");
				val_buffer(b,mname);
				buffer_append(b," has invalid __unserialize function");
			}
			s = val_call1(s,data);
			add_ref(b,s);
			return s;
		}
	case 'h':
		{
			int i;
			vhash *h = (vhash*)alloc(sizeof(vhash));
			h->ncells = read_int(b);
			h->nitems = read_int(b);
			h->cells = (hcell**)alloc(sizeof(hcell*)*h->ncells);
			for(i=0;i<h->ncells;i++)
				h->cells[i] = NULL;
			for(i=0;i<h->nitems;i++) {
				hcell **p;
				hcell *c = (hcell*)alloc(sizeof(hcell));
				c->hkey = read_int(b);
				c->key = unserialize_rec(b,loader);
				c->val = unserialize_rec(b,loader);
				c->next = NULL;
				p = &h->cells[c->hkey % h->ncells];
				while( *p != NULL )
					p = &(*p)->next;
				*p = c;
			}
			return alloc_abstract(k_hash,h);
		}
	default:
		ERROR();
		return val_null;
	}
}

/**
	unserialize : string -> #loader -> any
	<doc>Unserialize a stored value.
	Need a loader to look for modules if some bytecode functions have been serialized.
	</doc>
**/
static value unserialize( value s, value loader ) {
	sbuffer b;
	val_check(s,string);
	b.cur = (unsigned char*)val_string(s);
	b.pos = 0;
	b.olds = NULL;
	b.trefs = NULL;
	b.tsize = 0;
	b.nrefs = 0;
	b.size = val_strlen(s);
	b.totlen = 0;
	return unserialize_rec(&b,loader);
}

DEFINE_PRIM(serialize,1);
DEFINE_PRIM(unserialize,2);

/* ************************************************************************ */
