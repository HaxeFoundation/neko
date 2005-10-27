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

static value string_split( value o, value s ) {
	value l, first;
	int ilen;
	int slen;
	int start = 0;
	int pos;
	val_check(s,string);
	val_check(o,string);
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
	return (first == NULL)?val_null:first;
}

#define HEX			1
#define HEX_SMALL	2

static value sprintf( value fmt, value params ) {
	char *last, *cur, *end;
	int count = 0;
	buffer b;
	val_check(fmt,string);
	b = alloc_buffer(NULL);
	last = val_string(fmt);
	cur = last;
	end = cur + val_strlen(fmt);
	while( cur != end ) {
		if( *cur == '%' ) {
			int width = 0, prec = 0, flags = 0;
			buffer_append_sub(b,last,cur - last);
			cur++;
			while( *cur >= '0' && *cur <= '9' ) {
				width = width * 10 + (*cur - '0');
				cur++;
			}			
			if( *cur == '.' ) {
				cur++;
				while( *cur >= '0' && *cur <= '9' ) {
					prec = prec * 10 + (*cur - '0');
					cur++;
				}
			}
			if( *cur == '%' ) {
				buffer_append_sub(b,"%",1);
				cur++;
			} else {
				value param;
				if( count == 0 && !val_is_array(params) ) { // first ?
					param = params;
					count++;
				} else if( !val_is_array(params) || val_array_size(params) <= count )
					neko_error();
				else 
					param = val_array_ptr(params)[count++];
				switch( *cur ) {
				case 'c':
					{
						int c;
						char cc;
						val_check(param,int);
						c = val_int(param);
						if( c < 0 || c > 255 )
							neko_error();
						cc = (char)c;
						buffer_append_sub(b,&cc,1);
					}
					break;
				case 'x':
					flags |= HEX_SMALL;
				case 'X':
					flags |= HEX;
				case 'd':
					{
						char tmp[10];
						int sign = 0;
						int size = 0;
						int tsize;
						int n;
						val_check(param,int);
						n = val_int(param);
						if( !(flags & HEX) && n < 0 ) {
							sign++;
							prec--;
							n = -n;
						} else if( n == 0 )
							tmp[9-size++] = '0';
						if( flags & HEX ) {
							unsigned int nn = (unsigned int)n;
							while( nn > 0 ) {
								int k = nn&15;
								if( k < 10 )
									tmp[9-size++] = k + '0';
								else
									tmp[9-size++] = (k - 10) + ((flags & HEX_SMALL)?'a':'A');
								nn = nn >> 4;
							}
						} else {
							while( n > 0 ) {
								tmp[9-size++] = (n % 10) + '0';
								n = n / 10;
							}
						}
						tsize = (size > prec)?size:prec + sign;
						while( width > tsize ) {
							width--;
							buffer_append_sub(b," ",1);
						}
						if( sign )
							buffer_append_sub(b,"-",1);
						while( prec > size ) {
							prec--;
							buffer_append_sub(b,"0",1);
						}
						buffer_append_sub(b,tmp+10-size,size);
					}
					break;
				case 's':
					{
						int size;
						int tsize;
						val_check(param,string);
						size = val_strlen(param);
						tsize = (size > prec)?size:prec;
						while( width > tsize ) {
							width--;
							buffer_append_sub(b," ",1);
						}
						while( prec > size ) {
							prec--;
							buffer_append_sub(b," ",1);
						}
						buffer_append_sub(b,val_string(param),size);
					}
					break;
				case 'b':
					{
						val_check(param,bool);
						buffer_append_sub(b,val_bool(param)?"true":"false",val_bool(param)?4:5);
					}
					break;
				default:
					neko_error();
					break;
				}
			}
			cur++;
			last = cur;
		} else
			cur++;
	}
	buffer_append_sub(b,last,cur - last);
	return buffer_to_string(b);
}

static value test() {
	val_print(alloc_string("Calling a function inside std library...\n"));
	return val_null;
}

DEFINE_PRIM(sprintf,2);
DEFINE_PRIM(string_split,2);
DEFINE_PRIM(test,0);

/* ************************************************************************ */
