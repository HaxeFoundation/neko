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
	const objcell *c;
	field cid;
	min = 0;
	max = t->count;
	c = (const objcell*)t->cells;
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
