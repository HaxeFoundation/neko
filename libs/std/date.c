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
#include <time.h>
#include <stdio.h>
#include <memory.h>

extern field id_h;
extern field id_m;
extern field id_s;
extern field id_y;
extern field id_d;

static value date_now() {
	int t = (int)time(NULL);
	return alloc_int32(t);
}

static value date_new( value s ) {
	int o;
	if( val_is_null(s) )
		o = (int)time(NULL);
	else {
		struct tm t;
		bool recal = true;
		val_check(s,string);
		memset(&t,0,sizeof(struct tm));
		switch( val_strlen(s) ) {
		case 19:
			sscanf(val_string(s),"%4d-%2d-%2d %2d:%2d:%2d",&t.tm_year,&t.tm_mon,&t.tm_mday,&t.tm_hour,&t.tm_min,&t.tm_sec);
			t.tm_isdst = -1;
			break;
		case 8:
			sscanf(val_string(s),"%2d:%2d:%2d",&t.tm_hour,&t.tm_min,&t.tm_sec);
			o = t.tm_sec + t.tm_min * 60 + t.tm_hour * 60 * 60;
			recal = false;
			break;
		case 10:
			sscanf(val_string(s),"%4d-%2d-%2d",&t.tm_year,&t.tm_mon,&t.tm_mday);
			break;
		default:
			{
				buffer b = alloc_buffer("Invalid date format : ");
				val_buffer(b,s);
				bfailure(b);
			}
		}
		if( recal ) {
			t.tm_year -= 1900;
			t.tm_mon--;
			o = (int)mktime(&t);
		}
	}
	return alloc_int32(o);
}

static value date_format( value o, value fmt ) {
	char buf[128];
	struct tm *t;
	time_t d;
	val_check(o,int32);
	if( val_is_null(fmt) )
		fmt = alloc_string("%Y-%m-%d %H:%M:%S");
	val_check(fmt,string);	
	d = val_int32(o);
	t = localtime(&d);
	if( t == NULL )
		neko_error();
	strftime(buf,127,val_string(fmt),t);
	return alloc_string(buf);
}

static value date_set_hour( value o, value h, value m, value s ) {
	struct tm *t;
	time_t d;
	val_check(o,int32);
	val_check(h,int);
	val_check(m,int);
	val_check(s,int);
	d = val_int32(o);
	t = localtime(&d);
	t->tm_hour = val_int(h);
	t->tm_min = val_int(m);
	t->tm_sec = val_int(s);
	d = mktime(t);
	return alloc_int32(d);
}

static value date_set_day( value o, value y, value m, value d ) {
	struct tm *t;
	time_t date;
	val_check(o,int32);
	val_check(y,int);
	val_check(m,int);
	val_check(d,int);
	date = val_int32(o);
	t = localtime(&date);
	t->tm_year = val_int(y) - 1900;
	t->tm_mon = val_int(m) - 1;
	t->tm_mday = val_int(d);
	date = mktime(t);
	return alloc_int32(date);
}

static value date_get_day( value o ) {
	value r;
	struct tm *t;
	time_t d;
	val_check(o,int32);
	d = val_int32(o);
	t = localtime(&d);
	if( t == NULL )
		neko_error();
	r = alloc_object(NULL);
	alloc_field(r,id_y,alloc_int(t->tm_year + 1900));
	alloc_field(r,id_m,alloc_int(t->tm_mon + 1));
	alloc_field(r,id_d,alloc_int(t->tm_mday));
	return r;
}

static value date_get_hour( value o ) {
	value r;
	struct tm *t;
	time_t d;
	val_check(o,int32);
	d = val_int32(o);
	t = localtime(&d);
	if( t == NULL )
		neko_error();
	r = alloc_object(NULL);
	alloc_field(r,id_h,alloc_int(t->tm_hour));
	alloc_field(r,id_m,alloc_int(t->tm_min));
	alloc_field(r,id_s,alloc_int(t->tm_sec));
	return r;
}

DEFINE_PRIM(date_now,0);
DEFINE_PRIM(date_new,1);
DEFINE_PRIM(date_format,2);
DEFINE_PRIM(date_set_hour,4);
DEFINE_PRIM(date_set_day,4);
DEFINE_PRIM(date_get_hour,1);
DEFINE_PRIM(date_get_day,1);

/* ************************************************************************ */
