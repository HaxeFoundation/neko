/*
 * Copyright (C)2005-2012 Haxe Foundation
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
#include <time.h>
#include <stdio.h>
#include <memory.h>

/**
	<doc>
	<h1>Date</h1>
	<p>
	Date are using standard C functions in order to manipulate a 32 bit integer.
	Dates are then represented as the number of seconds elapsed since 1st January
	1970.
	</p>
	</doc>
**/

extern field id_h;
extern field id_m;
extern field id_s;
extern field id_y;
extern field id_d;

#ifdef NEKO_WINDOWS

static struct tm *localtime_r( time_t *t, struct tm *r ) {
	struct tm *r2 = localtime(t);
	if( r2 == NULL ) return NULL;
	*r = *r2;
	return r;
}

static struct tm *gmtime_r( time_t *t, struct tm *r ) {
	struct tm *r2 = gmtime(t);
	if( r2 == NULL ) return NULL;
	*r = *r2;
	return r;
}

#endif

/**
	date_now : void -> 'int32
	<doc>Return current date and time</doc>
**/
static value date_now() {
	int t = (int)time(NULL);
	return alloc_int32(t);
}

/**
	date_new : string? -> 'int32
	<doc>
	Parse a date format. The following formats are accepted :
	<ul>
		<li>[null] : return current date and time</li>
		<li>[YYYY-MM-DD HH:MM:SS] : full date and time</li>
		<li>[YYYY-MM-DD] : date only (time will be set to midnight)</li>
		<li>[HH:MM:SS] : this represent an elapsed time. It will be corrected with timezone so you can subtract it from a date.</li>
	</ul>
	</doc>
**/
static value date_new( value s ) {
	int o = 0;
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
			t.tm_isdst = -1;
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

/**
	date_format : #int32 -> fmt:string? -> string
	<doc>Format a date using [strftime]. If [fmt] is [null] then default format is used</doc>
**/
static value date_format( value o, value fmt ) {
	char buf[128];
	struct tm t;
	time_t d;
	val_check(o,any_int);
	if( val_is_null(fmt) )
		fmt = alloc_string("%Y-%m-%d %H:%M:%S");
	val_check(fmt,string);
	d = val_any_int(o);
	if( localtime_r(&d,&t) == NULL )
		neko_error();
	strftime(buf,127,val_string(fmt),&t);
	return alloc_string(buf);
}

/**
	date_set_hour : #int32 -> h:int -> m:int -> s:int -> 'int32
	<doc>Change the time of a date. Return the modified date</doc>
**/
static value date_set_hour( value o, value h, value m, value s ) {
	struct tm t;
	time_t d;
	val_check(o,any_int);
	val_check(h,int);
	val_check(m,int);
	val_check(s,int);
	d = val_any_int(o);
	if( localtime_r(&d,&t) == NULL )
		neko_error();
	t.tm_hour = val_int(h);
	t.tm_min = val_int(m);
	t.tm_sec = val_int(s);
	d = mktime(&t);
	if( d == -1 )
		neko_error();
	return alloc_int32((int)d);
}

/**
	date_set_day : #int32 -> y:int -> m:int -> d:int -> 'int32
	<doc>Change the day of a date. Return the modified date</doc>
**/
static value date_set_day( value o, value y, value m, value d ) {
	struct tm t;
	time_t date;
	val_check(o,any_int);
	val_check(y,int);
	val_check(m,int);
	val_check(d,int);
	date = val_any_int(o);
	if( localtime_r(&date,&t) == NULL )
		neko_error();
	t.tm_year = val_int(y) - 1900;
	t.tm_mon = val_int(m) - 1;
	t.tm_mday = val_int(d);
	date = mktime(&t);
	if( date == -1 )
		neko_error();
	return alloc_int32((int)date);
}

/**
	date_get_day : #int32 -> { y => int, m => int, d => int }
	<doc>Return the year month and day of a date</doc>
**/
static value date_get_day( value o ) {
	value r;
	struct tm t;
	time_t d;
	val_check(o,any_int);
	d = val_any_int(o);
	if( localtime_r(&d,&t) == NULL )
		neko_error();
	r = alloc_object(NULL);
	alloc_field(r,id_y,alloc_int(t.tm_year + 1900));
	alloc_field(r,id_m,alloc_int(t.tm_mon + 1));
	alloc_field(r,id_d,alloc_int(t.tm_mday));
	return r;
}

/**
	date_get_hour : #int32 -> { h => int, m => int, s => int }
	<doc>Return the hour minutes and seconds of a date</doc>
**/
static value date_get_hour( value o ) {
	value r;
	struct tm t;
	time_t d;
	val_check(o,any_int);
	d = val_any_int(o);
	if( localtime_r(&d,&t) == NULL )
		neko_error();
	r = alloc_object(NULL);
	alloc_field(r,id_h,alloc_int(t.tm_hour));
	alloc_field(r,id_m,alloc_int(t.tm_min));
	alloc_field(r,id_s,alloc_int(t.tm_sec));
	return r;
}

/**
	date_get_tz : void -> int
	<doc>Return the local Timezone (in seconds)</doc>
**/
static value date_get_tz() {
	struct tm local;
	struct tm gmt;
	int diff;
	time_t raw = time(NULL);
	if( localtime_r(&raw, &local) == NULL || gmtime_r(&raw, &gmt) == NULL )
		neko_error();
	diff = (local.tm_hour - gmt.tm_hour) * 3600 + (local.tm_min - gmt.tm_min) * 60;
	// adjust for different days/years
	if( gmt.tm_year > local.tm_year || gmt.tm_yday > local.tm_yday )
		diff -= 24 * 3600;
	else if( gmt.tm_year < local.tm_year || gmt.tm_yday < local.tm_yday )
		diff += 24 * 3600;
	return alloc_int(diff);
}

DEFINE_PRIM(date_now,0);
DEFINE_PRIM(date_new,1);
DEFINE_PRIM(date_format,2);
DEFINE_PRIM(date_set_hour,4);
DEFINE_PRIM(date_set_day,4);
DEFINE_PRIM(date_get_hour,1);
DEFINE_PRIM(date_get_day,1);
DEFINE_PRIM(date_get_tz,0);

/* ************************************************************************ */
