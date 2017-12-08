/*
 * Copyright (C)2005-2017 Haxe Foundation
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
#include "neko.h"

typedef struct vlist {
	value v;
	struct vlist *next;
} vlist;

typedef struct vparam {
	int *h;
	vlist l;
} vparam;

#define HBIG(x)  *h = *h * 65599 + (x)
#define HSMALL(x) *h = *h * 19 + (x)

static void hash_obj_rec( value v, field f, void *_p );

static void hash_rec( value v, int *h, vlist *l ) {
	val_type t = val_type(v);
	switch( t ) {
	case VAL_INT:
		HBIG(val_int(v));
		break;
	case VAL_INT32:
		HBIG(val_int32(v));
		break;
	case VAL_NULL:
		HSMALL(0);
		break;
	case VAL_FLOAT:
		{ 
			int k = sizeof(tfloat);
			while( k )
				HSMALL(val_string(v)[--k]);
		}
		break;
	case VAL_BOOL:
		HSMALL(val_bool(v));
		break;
	case VAL_STRING:
		{
			int k = val_strlen(v);
			while( k )
				HSMALL(val_string(v)[--k]);
		}
		break;
	case VAL_OBJECT:
	case VAL_ARRAY:
		{
			vlist *tmp = l;
			int k = 0;
			while( tmp != NULL ) {
				if( tmp->v == v ) {
					HSMALL(k);
					return;
				}
				k = k + 1;
				tmp = tmp->next;
			}
		}
		if( t == VAL_OBJECT ) {
			vparam p;
			p.h = h;
			p.l.v = v;
			p.l.next = l;
			val_iter_fields(v,hash_obj_rec,&p);
			v = (value)((vobject*)v)->proto;
			if( v != NULL )
				hash_rec(v,h,&p.l);
		} else {
			vlist cur;
			int k = val_array_size(v);
			cur.v = v;
			cur.next = l;
			while( k )
				hash_rec(val_array_ptr(v)[--k],h,&cur);
		}
		break;
	default:
		// ignore since we want hashes to be stable wrt memory
		break;
	}
}

static void hash_obj_rec( value v, field f, void *_p ) {
	vparam *p = (vparam*)_p;
	int *h = p->h;
	HBIG((int)f);
	hash_rec(v,h,&p->l);
}

EXTERN int val_hash( value v ) {
	int h = 0;
	hash_rec(v,&h,NULL);
	return (((unsigned int)h) & 0x3FFFFFFF);
}

/* ************************************************************************ */


