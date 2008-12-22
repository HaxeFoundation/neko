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
#ifndef __OBJTABLE_H
#define __OBJTABLE_H
#include "neko.h"

static INLINE void otable_init(objtable *t) {
	t->count = 0;
	t->cells = NULL;
}

static INLINE value *otable_find(objtable *t,field id) {
	int min;
	int max;
	int mid;
	objcell *c;
	field cid;
	min = 0;
	max = t->count;
	c = t->cells;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = c[mid].id;
		if( cid < id )
			min = mid + 1;
		else if( cid > id )
			max = mid;
		else
			return &c[mid].v;
	}
	return NULL;
}

static INLINE value otable_get(objtable *t,field id) {
	int min;
	int max;
	int mid;
	objcell *c;
	field cid;
	min = 0;
	max = t->count;
	c = t->cells;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = c[mid].id;
		if( cid < id )
			min = mid + 1;
		else if( cid > id )
			max = mid;
		else
			return c[mid].v;
	}
	return val_null;
}

void otable_replace(objtable *t, field id, value data);
int otable_remove(objtable *t, field id);
void otable_optimize(objtable *t);
#define otable_count(t)	(t)->count

void otable_copy(objtable *t, objtable *target);
void otable_iter(objtable *t, void f( value data, field id, void *), void *p );

#endif
/* ************************************************************************ */
