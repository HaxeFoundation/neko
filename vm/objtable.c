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
#include <string.h>
#include "objtable.h"

int otable_remove( objtable *t, field id ) {
	int min = 0;
	int max = t->count;
	int mid;
	field cid;
	objcell *c = t->cells;
	if( !max )
		return 0;
	while( min < max ) {
		mid = (min + max) >> 1;
		cid = c[mid].id;
		if( cid < id )
			min = mid + 1;
		else if( cid > id )
			max = mid;
		else {
			t->count--;
			memmove(&c[mid], &c[mid + 1], (t->count - mid) * sizeof(objcell));
			c[t->count].v = val_null;
			return 1;
		}
	}
	return 0;
}

void otable_optimize( objtable *t ) {
	int max = t->count;
	int i;
	int cur = 0;
	objcell *c = t->cells;
	for(i=0;i<max;i++) {
		value v = c[i].v;
		if( v != val_null )
			c[cur++] = c[i];
	}
	for(i=cur;i<max;i++)
		c[i].v = NULL;
	t->count = cur;
}

void otable_replace( objtable *t, field id, value data ) {
	int min = 0;
	int max = t->count;
	int mid;
	field cid;
	objcell *c = t->cells;
	const size_t objcell_size = sizeof(objcell);
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
	c = (objcell*)alloc(objcell_size * (t->count + 1));
	memcpy(c, t->cells, mid * objcell_size);
	c[mid].id = id;
	c[mid].v = data;
	memcpy(&c[mid + 1], &t->cells[mid], (t->count - mid) * objcell_size);
	t->cells = c;
	t->count++;
}

void otable_copy( objtable *t, objtable *target ) {
	const size_t size = sizeof(objcell) * t->count;
	target->count = t->count;
	target->cells = (objcell*)alloc(size);
	memcpy(target->cells,t->cells,size);
}

void otable_iter(objtable *t, void f( value data, field id, void *), void *p ) {
	int i;
	const int n = (const int)t->count;
	objcell *c = t->cells;
	for(i=0;i<n;i++)
		f(c[i].v,c[i].id,p);
}

/* ************************************************************************ */
