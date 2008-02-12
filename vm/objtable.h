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

typedef struct {
	field id;
	value v;
} cell;

struct _objtable
{
	int count;
	cell *cells;
};

static INLINE value *otable_find(objtable t,field id) {
	int min;
	int max;
	int mid;
	field cid;
	if( !t->count )
		return NULL;
	max = t->count;
	min = 0;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = t->cells[mid].id;
		if( cid < id )
			min = mid + 1;
		else if( cid > id )
			max = mid;
		else
			return &t->cells[mid].v;
	}
	return NULL;
}

objtable otable_empty();
void otable_replace(objtable t, field id, value data);
int otable_remove(objtable t, field id);
void otable_optimize(objtable t);
#define otable_count(t)	(t)->count

objtable otable_copy(objtable t);
void otable_iter(objtable t, void f( value data, field id, void *), void *p );

#endif
/* ************************************************************************ */
