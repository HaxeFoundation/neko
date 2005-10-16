/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
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
#include <neko.h>
#include <string.h>
#define PCRE_STATIC
#include <pcre.h>

#define PCRE(o)		((pcredata*)val_data(o))
#define NMATCHS		(10 * 3)

typedef struct {
	value str;
	pcre *r;
	int nmatchs;
	int matchs[NMATCHS];
} pcredata;

DEFINE_KIND(k_regexp);

static field id_pos;
static field id_len;

static void free_regexp( value p ) {	
	pcre_free( PCRE(p)->r );
}

static value regexp_new( value s ) {
	val_check(s,string);
	{
		value v;
		const char *error;
		int err_offset;
		pcre *p = pcre_compile(val_string(s),0,&error,&err_offset,NULL);
		pcredata *pdata;
		if( p == NULL ) {
			buffer b = alloc_buffer("Regexp compilation error : ");
			buffer_append(b,error);
			buffer_append(b," in ");
			val_buffer(b,s);
			bfailure(b);
		}
		v = alloc_abstract(k_regexp,alloc(sizeof(pcredata)));
		pdata = PCRE(v);
		pdata->r = p;
		pdata->str = val_null;
		pdata->nmatchs = 0;
		pcre_fullinfo(p,NULL,PCRE_INFO_CAPTURECOUNT,&pdata->nmatchs);
		val_gc(v,free_regexp);
		return v;
	}	
}

static value regexp_match( value o, value s ) {
	pcredata *d;
	val_check_kind(o,k_regexp);
	val_check(s,string);
	d = PCRE(o);
	if( pcre_exec(d->r,NULL,val_string(s),val_strlen(s),0,0,d->matchs,NMATCHS) >= 0 ) {
		d->str = s;
		return val_true;
	} else {
		d->str = val_null;
		return val_false;
	}
}

static value regexp_exact_match( value o, value s ) {
	value v = regexp_match(o,s);
	if( v == val_true ) {
		pcredata *d = PCRE(o);
		if( d->matchs[0] == 0 && d->matchs[1] == val_strlen(s) )
			return val_true;
		d->str = val_null;
		return val_false;
	}
	return v;
	
}

static value do_replace( value o, value s, value s2, bool all ) {	
	val_check_kind(o,k_regexp);	
	val_check(s,string);
	val_check(s2,string);	
	{
		pcredata *d = PCRE(o);
		buffer b = alloc_buffer(NULL);
		int pos = 0;
		int len = val_strlen(s);
		const char *str = val_string(s);
		const char *str2 = val_string(s2);
		int len2 = val_strlen(s2);
		while( pcre_exec(d->r,NULL,str,len,pos,0,d->matchs,NMATCHS) >= 0 ) {
			buffer_append_sub(b,str+pos,d->matchs[0] - pos);
			buffer_append_sub(b,str2,len2);
			pos = d->matchs[1];
			if( !all )
				break;
		}
		d->str = val_null;
		buffer_append_sub(b,str+pos,len-pos);
		return buffer_to_string(b);
	}
}

static value regexp_replace( value o, value s, value s2 ) {	
	return do_replace(o,s,s2,false);
}

static value regexp_replace_all( value o, value s, value s2 ) {
	return do_replace(o,s,s2,true);
}

static value regexp_matched( value o, value n ) {
	pcredata *d;
	int m;
	val_check_kind(o,k_regexp);	
	d = PCRE(o);
	val_check(n,int);
	m = val_int(n);
	if( m < 0 || m > d->nmatchs || d->str == NULL )
		neko_error();
	{
		int start = d->matchs[m*2];
		int len = d->matchs[m*2+1] - start;
		value str = alloc_empty_string(len);
		memcpy((char*)val_string(str),val_string(d->str)+start,len);
		return str;
	}
}

static value regexp_matched_pos( value o, value n ) {
	pcredata *d;
	int m;
	val_check_kind(o,k_regexp);	
	d = PCRE(o);
	val_check(n,int);
	m = val_int(n);
	if( m < 0 || m > d->nmatchs || val_is_null(d->str) )
		neko_error();
	{
		int start = d->matchs[m*2];
		int len = d->matchs[m*2+1] - start;
		value o = alloc_object(NULL);
		alloc_field(o,id_pos,alloc_int(start));
		alloc_field(o,id_len,alloc_int(len));
		return o;
	}
}

static void init() {
	id_pos = val_id("pos");
	id_len = val_id("len");	
}

DEFINE_PRIM(regexp_new,1);
DEFINE_PRIM(regexp_match,2);
DEFINE_PRIM(regexp_exact_match,2);
DEFINE_PRIM(regexp_replace,3);
DEFINE_PRIM(regexp_replace_all,3);
DEFINE_PRIM(regexp_matched,2);
DEFINE_PRIM(regexp_matched_pos,2);
DEFINE_ENTRY_POINT(init);

/* ************************************************************************ */
