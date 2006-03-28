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
#include <neko_mod.h>
#include <objtable.h>

typedef struct _vlist {
	void *v;
	struct _vlist *next;
} vlist;

typedef struct {
	vlist **l;
	int t;
} vparams;

static int mem_cache( void *v, vlist **l ) {
	vlist *p = *l;
	while( p != NULL ) {
		if( p->v == v )
			return 1;
		p = p->next;
	}
	p = (vlist*)alloc(sizeof(vlist));
	p->v = v;
	p->next = *l;
	*l = p;
	return 0;
}

static void mem_obj_field( value v, field f, void *_p );
static int mem_module( neko_module *m, vlist **l );

static int mem_size_rec( value v,  vlist **l ) {
	switch( val_type(v) ) {
	case VAL_INT:
	case VAL_BOOL:
	case VAL_NULL:
		return 0;
	case VAL_FLOAT:
		if( mem_cache(v,l) )
			return 0;
		return sizeof(vfloat);
	case VAL_STRING:
		if( mem_cache(v,l) )
			return 0;
		return sizeof(value) + val_strlen(v);
	case VAL_OBJECT:
		if( mem_cache(v,l) )
			return 0;
		{
			vparams p;
			p.l = l;
			p.t = sizeof(vobject);
#ifdef COMPACT_TABLE
			p.t += sizeof(struct _objtable);
#endif
			val_iter_fields(v,mem_obj_field,&p);
			if( ((vobject*)v)->proto != NULL )
				p.t += mem_size_rec((value)((vobject*)v)->proto,l);
			return p.t;
		}
	case VAL_ARRAY:
		if( mem_cache(v,l) )
			return 0;
		{
			int t = sizeof(value);
			int size = val_array_size(v);
			int i;
			t += size * sizeof(value);
			for(i=0;i<size;i++)
				t += mem_size_rec(val_array_ptr(v)[i],l);
			return t;
		}
	case VAL_FUNCTION:
		if( mem_cache(v,l) )
			return 0;
		{
			int t = sizeof(vfunction);
			t += mem_size_rec(((vfunction*)v)->env,l);
			if( val_tag(v) == VAL_PRIMITIVE )
				t += mem_size_rec((value)((vfunction*)v)->module,l);
			else
				t += mem_module(((vfunction*)v)->module,l);
			return t;
		}
	case VAL_ABSTRACT:
		{
			int t = sizeof(vabstract);
			if( val_kind(v) == neko_kind_module )
				t += mem_module((neko_module*)val_data(v),l);
			else if( val_kind(v) == k_hash ) {
				vhash *h = (vhash*)val_data(v);				
				int i;
				t += sizeof(vhash);
				t += sizeof(hcell*) * h->ncells;
				for(i=0;i<h->ncells;i++) {
					hcell *c = h->cells[i];
					while( c != NULL ) {
						t += sizeof(hcell);
						t += mem_size_rec(c->key,l);
						t += mem_size_rec(c->val,l);
						c = c->next;
					}
				}
			}	
			return t;
		}
	default:
		val_throw(alloc_string("mem_size : Unexpected value"));
		break;
	}
	return 0;
}

static void mem_obj_field( value v, field f, void *_p ) {
	vparams *p = (vparams*)_p;
#ifndef COMPACT_TABLE
	p->t += sizeof(struct _objtable);
#else
	p->t += sizeof(cell);
#endif
	p->t += mem_size_rec(v,p->l);
}

static int mem_module( neko_module *m, vlist **l ) {
	int t = 0;
	unsigned int i;
	if( mem_cache(m,l) )
		return 0;
	t += sizeof(neko_module);
	t += m->codesize * sizeof(int_val);
	t += m->nglobals * sizeof(int_val);
	for(i=0;i<m->nglobals;i++)
		t += mem_size_rec(m->globals[i],l);
	t += m->nfields * sizeof(value*);
	for(i=0;i<m->nfields;i++)
		t += mem_size_rec(m->fields[i],l);
	t += mem_size_rec(m->loader,l);
	t += mem_size_rec(m->exports,l);
	t += mem_size_rec(m->debuginf,l);
	t += mem_size_rec(m->name,l);
	return t;
}

static value mem_size( value v ) {
	vlist *l = NULL;
	return alloc_int(mem_size_rec(v,&l));
}

DEFINE_PRIM(mem_size,1);

/* ************************************************************************ */
