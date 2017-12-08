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
#include <neko_vm.h>

/**
	<doc>
	<h1>Misc</h1>
	<p>
	Misc. functions for different usages.
	</p>
	</doc>
**/

/**
	float_bytes : number -> bigendian:bool -> string
	<doc>Returns the 4 bytes representation of the number as an IEEE 32-bit float</doc>
**/
static value float_bytes( value n, value be ) {
	float f;
	val_check(n,number);
	val_check(be,bool);
	f = (float)val_number(n);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0];	c[0] = c[3]; c[3] = tmp;
		tmp = c[1];	c[1] = c[2]; c[2] = tmp;
	}
	return copy_string((char *)&f,4);
}

/**
	double_bytes : number -> bigendian:bool -> string
	<doc>Returns the 8 bytes representation of the number as an IEEE 64-bit float</doc>
**/
static value double_bytes( value n, value be ) {
	double f;
	val_check(n,number);
	val_check(be,bool);
	f = (double)val_number(n);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0]; c[0] = c[7]; c[7] = tmp;
		tmp = c[1];	c[1] = c[6]; c[6] = tmp;
		tmp = c[2]; c[2] = c[5]; c[5] = tmp;
		tmp = c[3];	c[3] = c[4]; c[4] = tmp;
	}
	return copy_string((char*)&f,8);
}

/**
	float_of_bytes : string -> bigendian:bool -> float
	<doc>Returns a float from a 4 bytes IEEE 32-bit representation</doc>
**/
static value float_of_bytes( value s, value be ) {
	float f;
	val_check(s,string);
	val_check(be,bool);
	if( val_strlen(s) != 4 )
		neko_error();
	f = *(float*)val_string(s);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0];	c[0] = c[3]; c[3] = tmp;
		tmp = c[1];	c[1] = c[2]; c[2] = tmp;
	}
	return alloc_float(f);
}

/**
	double_of_bytes : string -> bigendian:bool -> float
	<doc>Returns a float from a 8 bytes IEEE 64-bit representation</doc>
**/
static value double_of_bytes( value s, value be ) {
	double f;
	val_check(s,string);
	val_check(be,bool);
	if( val_strlen(s) != 8 )
		neko_error();
	f = *(double*)val_string(s);
	if( neko_is_big_endian() != val_bool(be) ) {
		char *c = (char*)&f;
		char tmp;
		tmp = c[0]; c[0] = c[7]; c[7] = tmp;
		tmp = c[1];	c[1] = c[6]; c[6] = tmp;
		tmp = c[2]; c[2] = c[5]; c[5] = tmp;
		tmp = c[3];	c[3] = c[4]; c[4] = tmp;
	}
	return alloc_float(f);
}

/**
	run_gc : major:bool -> void
	<doc>Run the Neko garbage collector</doc>
**/
static value run_gc( value b ) {
	val_check(b,bool);
	if( val_bool(b) )
		neko_gc_major();
	else
		neko_gc_loop();
	return val_null;
}

/**
	gc_stats : void -> { heap => int, free => int }
	<doc>Return the size of the GC heap and the among of free space, in bytes</doc>
**/
static value gc_stats() {
	int heap, free;
	value o;
	neko_gc_stats(&heap,&free);
	o = alloc_object(NULL);
	alloc_field(o,val_id("heap"),alloc_int(heap));
	alloc_field(o,val_id("free"),alloc_int(free));
	return o;
}

/**
	enable_jit : ?bool -> ?bool
	<doc>Enable or disable the JIT. Calling enable_jit(null) tells if JIT is enabled or not</doc>
**/
static value enable_jit( value b ) {
	if( val_is_null(b) )
		return alloc_bool(neko_vm_jit(neko_vm_current(),-1));
	val_check(b,bool);
	neko_vm_jit(neko_vm_current(),val_bool(b));
	return val_null;
}

/**
	test : void -> void
	<doc>The test function, to check that library is reachable and correctly linked</doc>
**/
static value test() {
	val_print(alloc_string("Calling a function inside std library...\n"));
	return val_null;
}

/**
	print_redirect : function:1? -> void
	<doc>
	Set a redirection function for all printed values. 
	Setting it to null will cancel the redirection and restore previous printer.
	</doc>
**/

static void print_callback( const char *s, int size, void *f ) {	
	val_call1(f,copy_string(s,size));
}

static value print_redirect( value f ) {
	neko_vm *vm = neko_vm_current();
	if( val_is_null(f) ) {
		neko_vm_redirect(vm,NULL,NULL);
		return val_null;
	}
	val_check_function(f,1);
	neko_vm_redirect(vm,print_callback,f);
	return val_null;
}

/**
	set_trusted : bool -> void
	<doc>
	Change the trusted mode of the VM.
	This can optimize some operations such as module loading by turning off some checks.
	</doc>
**/
static value set_trusted( value b ) {
	val_check(b,bool);
	neko_vm_trusted(neko_vm_current(),val_bool(b));
	return val_null;
}

/**
	same_closure : any -> any -> bool
	<doc>
	Compare two functions by checking that they refer to the same implementation and that their environments contains physically equal values.
	</doc>
**/
static value same_closure( value _f1, value _f2 ) {
	vfunction *f1 = (vfunction*)_f1;
	vfunction *f2 = (vfunction*)_f2;
	int i;
	if( !val_is_function(f1) || !val_is_function(f2) )
		return val_false;
	if( f1 == f2 )
		return val_true;
	if( f1->nargs != f2->nargs || f1->addr != f2->addr || f1->module != f2->module || val_array_size(f1->env) != val_array_size(f2->env) )
		return val_false;
	for(i=0;i<val_array_size(f1->env);i++)
		if( val_array_ptr(f1->env)[i] != val_array_ptr(f2->env)[i] )
			return val_false;
	return val_true;
}

// ------------- MERGE SORT HELPERS -----------------------------

typedef struct {
	value *arr;
	value cmp;
} m_sort;

static int ms_compare( m_sort *m, int a, int b ) {
	value v = val_call2(m->cmp,m->arr[a],m->arr[b]);
	if( !val_is_int(v) ) return -1;
	return val_int(v);
}

static void ms_swap( m_sort *m, int a, int b ) {
	value tmp = m->arr[a];
	m->arr[a] = m->arr[b];
	m->arr[b] = tmp;
}

static int ms_lower( m_sort *m, int from, int to, int val ) {
  	int len = to - from, half, mid;
  	while( len > 0 ) {
  		half = len>>1;
		mid = from + half;
		if( ms_compare(m, mid, val) < 0 ) {
    		from = mid+1;
    		len = len - half -1;
   		} else
   			len = half;
  	}
	return from;
}

static int ms_upper( m_sort *m, int from, int to, int val ) {
	int len = to - from, half, mid;
	while( len > 0 ) {
		half = len>>1;
		mid = from + half;
		if( ms_compare(m, val, mid) < 0 )
			len = half;
		else {
			from = mid+1;
			len = len - half -1;
		}
	}
	return from;
}

static int ms_gcd( int m, int n ) {
 	while( n != 0 ) {
		int t = m % n;
		m=n; n=t;
	}
 	return m;
 }


static void ms_rotate( m_sort *m, int from, int mid, int to ) {
	int n;
	if( from==mid || mid==to ) return;
	n = ms_gcd(to - from, mid - from);
	while (n-- != 0) {
		value val = m->arr[from+n];
		int shift = mid - from;
		int p1 = from+n, p2=from+n+shift;
		while (p2 != from + n) {
			m->arr[p1] = m->arr[p2];
			p1=p2;
			if( to - p2 > shift) p2 += shift;
			else p2=from + (shift - (to - p2));
		}
		m->arr[p1] = val;
	}
}

static void ms_do_merge( m_sort *m, int from, int pivot, int to, int len1, int len2 ) {
	int first_cut, second_cut, len11, len22, new_mid;
	if( len1 == 0 || len2==0 )
		return;
	if( len1+len2 == 2 ) {
		if( ms_compare(m, pivot, from) < 0 )
			ms_swap(m, pivot, from);
   		return;
  	}
	if (len1 > len2) {
		len11=len1>>1;
		first_cut = from + len11;
		second_cut = ms_lower(m, pivot, to, first_cut);
		len22 = second_cut - pivot;
	} else {
		len22 = len2>>1;
		second_cut = pivot + len22;
		first_cut = ms_upper(m, from, pivot, second_cut);
		len11=first_cut - from;
	}
	ms_rotate(m, first_cut, pivot, second_cut);
	new_mid=first_cut+len22;
	ms_do_merge(m, from, first_cut, new_mid, len11, len22);
	ms_do_merge(m, new_mid, second_cut, to, len1 - len11, len2 - len22);
}

static void merge_sort_rec( m_sort *m, int from, int to ) {
	int middle;
	if( to - from < 12 ) {
		// insert sort
		int i;
		if( to <= from ) return;
		for(i=from+1;i<to;i++) {
			int j = i;
			while( j > from ) {
				if( ms_compare(m,j,j-1) < 0 )
					ms_swap(m,j-1,j);
		    	else
		    		break;
			    j--;
			}
   		}
		return;
	}
	middle = (from + to)>>1;
	merge_sort_rec(m, from, middle);
	merge_sort_rec(m, middle, to);
	ms_do_merge(m, from, middle, to, middle-from, to - middle);
}

/**
	merge_sort : array -> length:int -> cmp:function:2 -> void
	<doc>
	Sort the array using stable in-place merge sort and the [cmp] compare function.
	</doc>
**/
static value merge_sort( value arr, value len, value cmp ) {
	m_sort m;
	val_check(arr,array);
	val_check(len,int);
	val_check_function(cmp,2);
	m.arr = val_array_ptr(arr);
	m.cmp = cmp;
	merge_sort_rec(&m,0,val_int(len));
	return val_null;
}

DEFINE_PRIM(float_bytes,2);
DEFINE_PRIM(double_bytes,2);
DEFINE_PRIM(float_of_bytes,2);
DEFINE_PRIM(double_of_bytes,2);
DEFINE_PRIM(run_gc,1);
DEFINE_PRIM(gc_stats,0);
DEFINE_PRIM(enable_jit,1);
DEFINE_PRIM(test,0);
DEFINE_PRIM(print_redirect,1);
DEFINE_PRIM(set_trusted,1);
DEFINE_PRIM(same_closure,2);
DEFINE_PRIM(merge_sort,3);

/* ************************************************************************ */
