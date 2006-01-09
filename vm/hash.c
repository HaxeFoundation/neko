/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
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


