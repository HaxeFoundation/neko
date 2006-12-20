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

#ifndef COMPACT_TABLE

struct _objtable {
	field id;
	value data;
	struct _objtable *left;
	struct _objtable *right;
	int skew;
};

value *otable_find(objtable t,field id);

#define otable_empty()	NULL
#define otable_replace(t,id,data) _otable_replace(&(t),id,data)
#define otable_remove(t,id) (_otable_remove(&(t),id) != (unsigned int)-1)
#define otable_optimize(t) _otable_optimize(&(t))

int otable_count(objtable t);
unsigned int _otable_replace(objtable *t, field id, value data);
unsigned int _otable_remove(objtable *t, field id);
void _otable_optimize(objtable *t);


#else

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

#endif

objtable otable_copy(objtable t);
void otable_iter(objtable t, void f( value data, field id, void *), void *p );

#endif
/* ************************************************************************ */
