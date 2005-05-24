/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#pragma once
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

INLINE value *otable_find(objtable t,field id) {
	int min = 0;
	int max = t->count;
	int mid;
	field cid;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = t->cells[mid].id;
		if( cid < id )
			max = mid;
		else if( cid > id )
			min = mid;
		else
			return &t->cells[mid].v;
	}
	return NULL;
}

objtable otable_empty();
void otable_replace(objtable t, field id, value data);
int otable_remove(objtable t, field id);
void otable_optimize(objtable t);
objtable otable_copy(objtable t);

/* ************************************************************************ */
