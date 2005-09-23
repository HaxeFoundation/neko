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
#include <neko.h>

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
	switch( val_type(v) ) {
	case VAL_INT:
		HBIG(val_int(v));
		break;
	case VAL_NULL:
		HSMALL(0);
		break;
	case VAL_FLOAT:
		k = sizeof(tfloat);
		while( k )
			HSMALL(val_string(v)[--k]);
		break;
	case VAL_BOOL:
		HSMALL(val_bool(v));
		break;
	case VAL_STRING:
		k = val_strlen(v);
		while( k )
			HSMALL(val_string(v)[--k]);
		break;
	case VAL_OBJECT:
		{
			vparam p;
			p.h = h;
			p.l.v = v;
			p.l.next = l;
			val_iter_fields(v,hash_obj_rec,&p);
		}
		break;
	case VAL_ARRAY:
		k = val_array_size(v);
		{
			vlist cur;
			cur.v = v;
			cur.next = l;
			while( k )
				hash_rec(val_array_ptr(v)[--k],h,&cur);
		}
		break;
	default:
		HBIG((int)v);
		break;
	}
}

static void hash_obj_rec( value v, field f, void *_p ) {
	vparam *p = (vparam*)_p;
	int *h = p->h;
	HBIG((int)f);
	hash_rec(v,h,&p->l);
}

static value hash( value v ) {
	int h = 0;
	hash_rec(v,&h,NULL);
	return alloc_int(h & 0x3FFFFFFF);
}

DEFINE_PRIM(hash,1);

/* ************************************************************************ */


