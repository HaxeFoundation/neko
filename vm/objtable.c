/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
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
#include "objtable.h"

#ifndef COMPACT_TABLE

#define BAL_NONE	0
#define BAL_LEFT	1
#define BAL_RIGHT	2

#define ERROR		((uint_val)-1)
#define DONE		0
#define OK			1
#define BALANCE		2

INLINE void avlrotleft(objtable *n) {
	objtable tmp = *n;
	*n = (*n)->right;
	tmp->right = (*n)->left;
	(*n)->left = tmp;
}

INLINE void avlrotright(objtable *n) {
	objtable tmp = *n;
	*n = (*n)->left;
	tmp->left = (*n)->right;
	(*n)->right = tmp;
}

INLINE uint_val avlleftgrown(objtable *n) {
	switch( (*n)->skew ) {
	case BAL_LEFT:
		if( (*n)->left->skew == BAL_LEFT ) {
			(*n)->skew = (*n)->left->skew = BAL_NONE;
			avlrotright(n);
		}	
		else {
			switch( (*n)->left->right->skew ) {
			case BAL_LEFT:
				(*n)->skew = BAL_RIGHT;
				(*n)->left->skew = BAL_NONE;
				break;
			case BAL_RIGHT:
				(*n)->skew = BAL_NONE;
				(*n)->left->skew = BAL_LEFT;
				break;
			default:
				(*n)->skew = BAL_NONE;
				(*n)->left->skew = BAL_NONE;
				break;
			}
			(*n)->left->right->skew = BAL_NONE;
			avlrotleft(&(*n)->left);
			avlrotright(n);
		}
		return OK;
	case BAL_RIGHT:
		(*n)->skew = BAL_NONE;
		return OK;
	default:
		(*n)->skew = BAL_LEFT;
		return BALANCE;
	}
}

INLINE uint_val avlrightgrown(objtable *n) {
	switch( (*n)->skew ) {
	case BAL_LEFT:					
		(*n)->skew = BAL_NONE;
		return OK;
	case BAL_RIGHT:
		if( (*n)->right->skew == BAL_RIGHT ) {
			(*n)->skew = (*n)->right->skew = BAL_NONE;
			avlrotleft(n);
		}
		else {
			switch( (*n)->right->left->skew ) {
			case BAL_RIGHT:
				(*n)->skew = BAL_LEFT;
				(*n)->right->skew = BAL_NONE;
				break;
			case BAL_LEFT:
				(*n)->skew = BAL_NONE;
				(*n)->right->skew = BAL_RIGHT;
				break;
			default:
				(*n)->skew = BAL_NONE;
				(*n)->right->skew = BAL_NONE;
				break;
			}
			(*n)->right->left->skew = BAL_NONE;
			avlrotright(&(*n)->right);
			avlrotleft(n);
		}
		return OK;
	default:
		(*n)->skew = BAL_RIGHT;
		return BALANCE;
	}
}


uint_val _otable_replace(objtable *n, field id, value data) {
	uint_val tmp;
	if( !(*n) ) {
		*n = (objtable)alloc(sizeof(struct _objtable));
		(*n)->left = NULL;
		(*n)->right = NULL;
		(*n)->id = id;
		(*n)->data = data;
		(*n)->skew = BAL_NONE;
		return BALANCE;
	}
	if( id < (*n)->id ) {
		if( (tmp = _otable_replace(&(*n)->left,id,data)) == BALANCE )
			return avlleftgrown(n);		
		return tmp;
	}
	if( id > (*n)->id ) {
		if( (tmp = _otable_replace(&(*n)->right,id,data)) == BALANCE )
			return avlrightgrown(n);		
		return tmp;
	}
	(*n)->data = data;
	return DONE;
}


INLINE uint_val avlleftshrunk(objtable *n) {
	switch( (*n)->skew ) {
	case BAL_LEFT:
		(*n)->skew = BAL_NONE;
		return BALANCE;
	case BAL_RIGHT:
		if( (*n)->right->skew == BAL_RIGHT ) {
			(*n)->skew = (*n)->right->skew = BAL_NONE;
			avlrotleft(n);
			return BALANCE;
		}
		else if( (*n)->right->skew == BAL_NONE ) {
			(*n)->skew = BAL_RIGHT;
			(*n)->right->skew = BAL_LEFT;
			avlrotleft(n);
			return OK;
		}
		else {
			switch( (*n)->right->left->skew ) {
			case BAL_LEFT:
				(*n)->skew = BAL_NONE;
				(*n)->right->skew = BAL_RIGHT;
				break;
			case BAL_RIGHT:
				(*n)->skew = BAL_LEFT;
				(*n)->right->skew = BAL_NONE;
				break;
			default:
				(*n)->skew = BAL_NONE;
				(*n)->right->skew = BAL_NONE;
				break;
			}
			(*n)->right->left->skew = BAL_NONE;
			avlrotright(&(*n)->right);
			avlrotleft(n);
			return BALANCE;
		}
	default:
		(*n)->skew = BAL_RIGHT;
		return OK;
	}

}

INLINE uint_val avlrightshrunk(objtable *n) {
	switch( (*n)->skew ) {
	case BAL_RIGHT:
		(*n)->skew = BAL_NONE;
		return BALANCE;
	case BAL_LEFT:
		if( (*n)->left->skew == BAL_LEFT ) {
			(*n)->skew = (*n)->left->skew = BAL_NONE;
			avlrotright(n);
			return BALANCE;
		}
		else if( (*n)->left->skew == BAL_NONE ) {
			(*n)->skew = BAL_LEFT;
			(*n)->left->skew = BAL_RIGHT;
			avlrotright(n);
			return OK;
		}
		else {
			switch( (*n)->left->right->skew ) {
			case BAL_LEFT:
				(*n)->skew = BAL_RIGHT;
				(*n)->left->skew = BAL_NONE;
				break;
			case BAL_RIGHT:
				(*n)->skew = BAL_NONE;
				(*n)->left->skew = BAL_LEFT;	
				break;
			default:
				(*n)->skew = BAL_NONE;
				(*n)->left->skew = BAL_NONE;
				break;
			}
			(*n)->left->right->skew = BAL_NONE;
			avlrotleft(&(*n)->left);
			avlrotright(n);
			return BALANCE;
		}
	default:
		(*n)->skew = BAL_LEFT;
		return OK;
	}
}

INLINE int_val avlfindhighest(objtable target, objtable *n, uint_val *res)  {
	objtable tmp;
	*res = BALANCE;
	if( !(*n) )
		return 0;
	
	if( (*n)->right ) {
		if (!avlfindhighest(target, &(*n)->right, res))
			return 0;		
		if (*res == BALANCE)
			*res = avlrightshrunk(n);
		return 1;
	}
	target->id  = (*n)->id;
	target->data = (*n)->data;
	tmp = *n;
	*n = (*n)->left;
	return 1;
}

INLINE int_val avlfindlowest(objtable target, objtable *n, uint_val *res) {
	objtable tmp;
	*res = BALANCE;
	if( !(*n) )
		return 0;	
	if( (*n)->left ) {
		if (!avlfindlowest(target, &(*n)->left, res))
			return 0;		
		if (*res == BALANCE)
			*res =  avlleftshrunk(n);
		return 1;
	}
	target->id = (*n)->id;
	target->data = (*n)->data;
	tmp = *n;
	*n = (*n)->right;
	return 1;
}

uint_val _otable_remove(objtable *n, field key) {
	uint_val tmp = BALANCE;
	if( !(*n) )
		return ERROR;
	if( key < (*n)->id ) {
		if( (tmp = _otable_remove(&(*n)->left,key)) == BALANCE )
			return avlleftshrunk(n);
		return tmp;
	}
	if( key > (*n)->id ) {
		if( (tmp = _otable_remove(&(*n)->right,key)) == BALANCE )
			return avlrightshrunk(n);
		return tmp;
	}
	if( (*n)->left ) {
		if( avlfindhighest(*n, &((*n)->left), &tmp) ) {
			if( tmp == BALANCE )
				tmp = avlleftshrunk(n);
			return tmp;
		}
	}
	if( (*n)->right ) {
		if( avlfindlowest(*n, &((*n)->right), &tmp) ) {
			if( tmp == BALANCE )
				tmp = avlrightshrunk(n);
			return tmp;
		}
	}
 	*n = NULL;
	return BALANCE;
}

value *otable_find(objtable n, field key) {
	if( !n )
		return NULL;
	if( key < n->id )
		return otable_find(n->left,key);
	if( key > n->id )
		return otable_find(n->right,key);
	return &n->data;
}

objtable otable_copy(objtable t) {
	objtable nt;
	if( t == NULL )
		return NULL;
	nt = (objtable)alloc(sizeof(struct _objtable));
	nt->id = t->id;
	nt->data = t->data;
	nt->skew = t->skew;
	nt->left = otable_copy(t->left);
	nt->right = otable_copy(t->right);
	return nt;
}

void otable_iter(objtable t, void f( value data, field id, void *), void *p ) {
	if( t == NULL )
		return;
	otable_iter(t->left,f,p);
	f(t->data,t->id,p);
	otable_iter(t->right,f,p);
}

static void otable_optrec( objtable t, objtable *t2 ) {
	if( t == NULL )
		return;
	if( !val_is_null(t->data) )
		otable_replace(*t2,t->id,t->data);
	otable_optrec(t->left,t2);
	otable_optrec(t->right,t2);
}

void _otable_optimize( objtable *t ) {
	objtable t2 = NULL;
	otable_optrec(*t,&t2);
	*t = t2;
}

int_val otable_count( objtable t ) {
	if( t == NULL )
		return 0;
	return 1 + otable_count(t->left) + otable_count(t->right);
}

#else

objtable otable_empty() {
	objtable t2 = (objtable)alloc(sizeof(struct _objtable));
	t2->count = 0;
	t2->cells = NULL;
	return t2;
}

void otable_remove( objtable t, field id ) {
	int_val min = 0;
	int_val max = t->count;
	int_val mid;
	field cid;
	cell *c = t->cells;
	if( !max )
		return;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = c[mid].id;
		if( cid < id )
			min = mid + 1;
		else if( cid > id )
			max = mid;
		else {
			t->count--;
			while( mid < t->count ) {
				c[mid] = c[mid+1];
				mid++;
			}
			c[mid].v = NULL;
			return;
		}
	}
	return;
}

void otable_optimize( objtable t ) {
	int_val max = t->count;
	int_val i;
	int_val cur = 0;
	cell *c = t->cells;
	for(i=0;i<max;i++) {
		value v = c[i].v;
		if( v != val_null )
			c[cur++] = c[i];
	}
	for(i=cur;i<max;i++)
		c[i].v = NULL;
	t->count = cur;
}

void otable_replace( objtable t, field id, value data ) {
	int_val min = 0;
	int_val max = t->count;
	int_val mid;
	field cid;
	cell *c = t->cells;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = c[mid].id;
		if( cid < id )
			min = mid + 1;
		else if( cid > id )
			max = mid;
		else {
			c[mid].v = data;
			return;
		}
	}
	mid = (min + max) >> 1;
	c = (cell*)alloc(sizeof(cell)*(t->count + 1));
	min = 0;
	while( min < mid ) {
		c[min] = t->cells[min];
		min++;
	}
	c[mid].id = id;
	c[mid].v = data;
	while( min < t->count ) {
		c[min+1] = t->cells[min];
		min++;
	}
	t->cells = c;
	t->count++;
}

objtable otable_copy( objtable t ) {
	objtable t2 = (objtable)alloc(sizeof(struct _objtable));
	t2->count = t->count;
	t2->cells = (cell*)alloc(sizeof(cell)*t->count);
	memcpy(t2->cells,t->cells,sizeof(cell)*t->count);
	return t2;
}

void otable_iter(objtable t, void f( value data, field id, void *), void *p ) {
	int_val i;
	cell *c = t->cells;
	for(i=0;i<t->count;i++)
		f(c[i].v,c[i].id,p);
}


#endif

/* ************************************************************************ */
