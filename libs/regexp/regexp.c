/*
 * Copyright (C)2005-2015 Haxe Foundation
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
#include <neko.h>
#include <string.h>
#define PCRE_STATIC
#include <pcre.h>

#define PCRE(o)		((pcredata*)val_data(o))

typedef struct {
	value str;
	pcre *r;
	int nmatchs;
	int *matchs;
} pcredata;

DEFINE_KIND(k_regexp);

static field id_pos;
static field id_len;
static pcre_extra limit;

/**
	<doc>
	<h1>Regexp</h1>
	<p>
	Regular expressions using PCRE engine.
	</p>
	</doc>
**/

static void free_regexp( value p ) {	
	pcre_free( PCRE(p)->r );
}

static int do_exec( pcredata *d, const char *str, int len, int pos ) {
	int res = pcre_exec(d->r,&limit,str,len,pos,0,d->matchs,d->nmatchs * 3);
	if( res >= 0 )
		return 1;
	if( res != PCRE_ERROR_NOMATCH )
		val_throw(alloc_string("An error occured while running pcre_exec"));
	return 0;
}

/**
	regexp_new_options : reg:string -> options:string -> 'regexp
	<doc>Build a new regexpr with the following options :
	<ul>
		<li>i : case insensitive matching</li>
		<li>s : . match anything including newlines</li>
		<li>m : treat the input as a multiline string</li>
		<li>u : run in utf8 mode</li>
		<li>g : turn off greedy behavior</li>
	</ul>
	</doc>
**/
static value regexp_new_options( value s, value opt ) {
	val_check(s,string);
	val_check(opt,string);
	{
		value v;
		const char *error;
		int err_offset;
		pcre *p;
		pcredata *pdata;
		char *o = val_string(opt);
		int options = 0;
		while( *o ) {
			switch( *o++ ) {
			case 'i':
				options |= PCRE_CASELESS;
				break;
			case 's':
				options |= PCRE_DOTALL;
				break;
			case 'm':
				options |= PCRE_MULTILINE;
				break;
			case 'u':
				options |= PCRE_UTF8;
				break;
			case 'g':
				options |= PCRE_UNGREEDY;
				break;
			default:
				neko_error();
				break;
			}
		}
		p = pcre_compile(val_string(s),options,&error,&err_offset,NULL);
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
		pdata->nmatchs++;
		pdata->matchs = (int*)alloc_private(sizeof(int) * 3 * pdata->nmatchs);
		val_gc(v,free_regexp);
		return v;
	}	
}

/**
	regexp_new : string -> 'regexp
	<doc>Build a new regexp</doc>
**/
static value regexp_new( value s ) {
	return regexp_new_options(s,alloc_string(""));
}

/**
	regexp_match : 'regexp -> string -> pos:int -> len:int -> bool
	<doc>Match [len] chars of a string starting at [pos] using the regexp.
	Return true if match found</doc>
**/
static value regexp_match( value o, value s, value p, value len ) {
	pcredata *d;
	int pp,ll;
	val_check_kind(o,k_regexp);
	val_check(s,string);
	val_check(p,int);
	val_check(len,int);
	pp = val_int(p);
	ll = val_int(len);
	if( pp < 0 || ll < 0 || pp > val_strlen(s) || pp + ll > val_strlen(s) )
		neko_error();
	d = PCRE(o);
	if( do_exec(d,val_string(s),ll+pp,pp) ) {
		d->str = s;
		return val_true;
	} else {
		d->str = val_null;
		return val_false;
	}
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
		while( do_exec(d,str,len,pos) ) {
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

/**
	regexp_replace : 'regexp -> from:string -> by:string -> string
	<doc>Perform a replacement using a regexp</doc>
**/
static value regexp_replace( value o, value s, value s2 ) {	
	return do_replace(o,s,s2,false);
}

/**
	regexp_replace_all : 'regexp -> from:string -> by:string -> string
	<doc>Perform a replacement of all matched substrings using a regexp</doc>
**/
static value regexp_replace_all( value o, value s, value s2 ) {
	return do_replace(o,s,s2,true);
}

/**
	regexp_replace_fun : 'regexp -> from:string -> f:('regexp -> any) -> string
	<doc>Perform a replacement of all matched substrings by calling [f] for every match</doc>
**/
static value regexp_replace_fun( value o, value s, value f ) {
	val_check_kind(o,k_regexp);
	val_check(s,string);
	val_check_function(f,1);
	{
		pcredata *d = PCRE(o);
		buffer b = alloc_buffer(NULL);
		int pos = 0;
		int len = val_strlen(s);
		const char *str = val_string(s);
		d->str = s;
		while( do_exec(d,str,len,pos) ) {
			buffer_append_sub(b,str+pos,d->matchs[0] - pos);
			val_buffer(b,val_call1(f,o));
			pos = d->matchs[1];
		}
		d->str = val_null;
		buffer_append_sub(b,str+pos,len-pos);
		return buffer_to_string(b);
	}
}

/**
	regexp_matched : 'regexp -> n:int -> string?
	<doc>Return the [n]th matched block by the regexp. If [n] is 0 then return 
	the whole matched substring. If the [n]th matched block was optional and not matched, returns null</doc>
**/
static value regexp_matched( value o, value n ) {
	pcredata *d;
	int m;
	val_check_kind(o,k_regexp);	
	d = PCRE(o);
	val_check(n,int);
	m = val_int(n);
	if( m < 0 || m >= d->nmatchs || val_is_null(d->str) )
		neko_error();
	{
		int start = d->matchs[m*2];
		int len = d->matchs[m*2+1] - start;
		value str;
		if( start == -1 )
			return val_null;
		str = alloc_empty_string(len);
		memcpy((char*)val_string(str),val_string(d->str)+start,len);
		return str;
	}
}

/**
	regexp_matched_pos : 'regexp -> n:int -> { pos => int, len => int }
	<doc>Return the [n]th matched block position by the regexp. If [n] is 0 then
	return the whole matched substring position</doc>
**/
static value regexp_matched_pos( value o, value n ) {
	pcredata *d;
	int m;
	val_check_kind(o,k_regexp);	
	d = PCRE(o);
	val_check(n,int);
	m = val_int(n);
	if( m < 0 || m >= d->nmatchs || val_is_null(d->str) )
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

void regexp_main() {
	id_pos = val_id("pos");
	id_len = val_id("len");	
	limit.flags = PCRE_EXTRA_MATCH_LIMIT_RECURSION;
	limit.match_limit_recursion = 3500; // adapted based on Windows 1MB stack size
}

DEFINE_PRIM(regexp_new,1);
DEFINE_PRIM(regexp_new_options,2);
DEFINE_PRIM(regexp_match,4);
DEFINE_PRIM(regexp_replace,3);
DEFINE_PRIM(regexp_replace_all,3);
DEFINE_PRIM(regexp_replace_fun,3);
DEFINE_PRIM(regexp_matched,2);
DEFINE_PRIM(regexp_matched_pos,2);
DEFINE_ENTRY_POINT(regexp_main);

/* ************************************************************************ */
