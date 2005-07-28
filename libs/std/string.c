#include <neko.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#ifdef _WIN32
#	include <direct.h>
#else
#	include <unistd.h>
#endif

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

static value set_locale(l) {
	if( !val_is_string(l) )
		return val_null;
	return alloc_bool(setlocale(LC_TIME,val_string(l)) != NULL);
}

static value get_cwd() {
	char buf[256];
	int l;
	if( getcwd(buf,256) == NULL )
		return val_null;
	l = strlen(buf);
	if( buf[l-1] != '/' && buf[l-1] != '\\' ) {
		buf[l] = '/';
		buf[l+1] = 0;
	}
	return alloc_string(buf);
}

static value sys_command( value cmd ) {
	if( !val_is_string(cmd) || val_strlen(cmd) == 0 )
		return val_null;
	return alloc_int( system(val_string(cmd)) );
}

DEFINE_PRIM(string_split,2);
DEFINE_PRIM(set_locale,1);
DEFINE_PRIM(get_cwd,0);
DEFINE_PRIM(sys_command,1);
