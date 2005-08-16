#include <neko.h>

DEFINE_KIND(k_buffer);

static value buffer_alloc() {
	buffer b = alloc_buffer(NULL);
	return alloc_abstract(k_buffer,b);
}

static value buffer_add( value b, value v ) {
	val_check_kind(b,k_buffer);
	val_buffer( (buffer)val_data(b), v );
	return val_true;
}

static value buffer_add_sub( value b, value v, value p, value l ) {
	val_check_kind(b,k_buffer);
	val_check(v,string);
	val_check(p,int);
	val_check(l,int);	
	if( val_int(p) < 0 || val_int(l) < 0 )
		return val_null;
	if( val_strlen(v) < val_int(p) || val_strlen(v) < val_int(p) + val_int(l) )
		return val_null;
	buffer_append_sub( (buffer)val_data(b), val_string(v) + val_int(p) , val_int(l) );
	return val_true;
}

static value buffer_string( value b ) {
	val_check_kind(b,k_buffer);
	return buffer_to_string( (buffer)val_data(b) );
}

DEFINE_PRIM(buffer_alloc,0);
DEFINE_PRIM(buffer_add,2);
DEFINE_PRIM(buffer_add_sub,4);
DEFINE_PRIM(buffer_string,1);
