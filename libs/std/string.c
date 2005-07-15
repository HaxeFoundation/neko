#include <neko.h>
#include <string.h>

static value string_split( value o, value s ) {
	value l, first;
	int ilen;
	int slen;
	int start = 0;
	int pos;	
	if( !val_is_string(s) || !val_is_string(o) )
		return val_null;
	ilen = val_strlen(o);
	slen = val_strlen(s);
	l = NULL;
	first = NULL;
	for(pos=0;pos<=ilen-slen;pos++)
		if( memcmp(val_string(o)+pos,val_string(s),slen) == 0 ) {
			value ss = copy_string(val_string(o)+start,pos-start);
			value l2 = alloc_array(2);
			val_array_ptr(l2)[0] = ss;
			val_array_ptr(l2)[1] = val_null;
			if( first == NULL )
				first = l2;
			else
				val_array_ptr(l)[1] = l2;
			l = l2;
			start = pos + slen;
			pos = start - 1;
		}
	if( ilen > 0 ) {
		value ss = copy_string(val_string(o)+start,ilen-start);
		value l2 = alloc_array(2);
		val_array_ptr(l2)[0] = ss;
		val_array_ptr(l2)[1] = val_null;
		if( first == NULL )
			first = l2;
		else
			val_array_ptr(l)[1] = l2;
	}
	return first;
}

DEFINE_PRIM(string_split,2);