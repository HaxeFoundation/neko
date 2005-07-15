#include <neko.h>
#include <time.h>
#include <stdio.h>
#include <memory.h>
#include <locale.h>

DEFINE_KIND(k_date);

#define val_date(o)		((time_t*)val_data(o))

extern field id_h;
extern field id_m;
extern field id_s;
extern field id_y;
extern field id_d;

static value date_now() {
	value o = alloc_abstract(k_date,alloc_private(sizeof(time_t)));
	*val_date(o) = time(NULL);
	return o;
}

static value date_new( value s ) {
	value o = alloc_abstract(k_date,alloc_private(sizeof(time_t)));
	if( val_is_null(s) )
		*val_date(o) = time(NULL);
	else {
		struct tm t;
		bool recal = true;
		if( !val_is_string(s) )
			return val_null;
		memset(&t,0,sizeof(struct tm));
		switch( val_strlen(s) ) {
		case 19:
			sscanf(val_string(s),"%4d-%2d-%2d %2d:%2d:%2d",&t.tm_year,&t.tm_mon,&t.tm_mday,&t.tm_hour,&t.tm_min,&t.tm_sec);
			t.tm_isdst = -1;
			break;
		case 8:
			sscanf(val_string(s),"%2d:%2d:%2d",&t.tm_hour,&t.tm_min,&t.tm_sec);
			*val_date(o) = t.tm_sec + t.tm_min * 60 + t.tm_hour * 60 * 60;
			recal = false;
			break;
		case 10:
			sscanf(val_string(s),"%4d-%2d-%2d",&t.tm_year,&t.tm_mon,&t.tm_mday);
			break;
		default:
			val_print(alloc_string("Invalid date format : "));
			val_print(s);
			return NULL;
		}
		if( recal ) {
			t.tm_year -= 1900;
			t.tm_mon--;
			*val_date(o) = mktime(&t);
		}
	}
	return o;
}

static value date_format( value o, value fmt ) {
	char buf[128];
	struct tm *t;
	val_check_kind(o,k_date);
	if( val_is_null(fmt) )
		fmt = alloc_string("%Y-%m-%d %H:%M:%S");
	if( !val_is_string(fmt) )
		return val_null;	
	t = localtime(val_date(o));
	if( t == NULL )
		return alloc_string("(date before 1970)");
	strftime(buf,127,val_string(fmt),t);
	return alloc_string(buf);
}

static value date_set_time( value o, value h, value m, value s ) {
	struct tm *t;
	val_check_kind(o,k_date);
	if( !val_is_int(h) || !val_is_int(m) || !val_is_int(s) )
		return val_null;
	t = localtime(val_date(o));
	t->tm_hour = val_int(h);
	t->tm_min = val_int(m);
	t->tm_sec = val_int(s);
	*val_date(o) = mktime(t);
	return val_true;
}

static value date_set_day( value o, value y, value m, value d ) {
	struct tm *t;
	val_check_kind(o,k_date);
	if( !val_is_int(y) || !val_is_int(m) || !val_is_int(d) )
		return val_null;
	t = localtime(val_date(o));
	t->tm_year = val_int(y) - 1900;
	t->tm_mon = val_int(m) - 1;
	t->tm_mday = val_int(d);
	*val_date(o) = mktime(t);
	return val_true;
}

static value date_get_day( value o ) {
	value r;
	struct tm *t;
	val_check_kind(o,k_date);
	t = localtime(val_date(o));
	if( t == NULL )
		return val_null;
	r = alloc_object(NULL);
	alloc_field(r,id_y,alloc_int(t->tm_year + 1900));
	alloc_field(r,id_m,alloc_int(t->tm_mon + 1));
	alloc_field(r,id_d,alloc_int(t->tm_mday));
	return r;
}

static value date_get_time( value o ) {
	value r;
	struct tm *t;
	val_check_kind(o,k_date);
	t = localtime(val_date(o));
	if( t == NULL )
		return val_null;
	r = alloc_object(NULL);
	alloc_field(r,id_h,alloc_int(t->tm_hour));
	alloc_field(r,id_m,alloc_int(t->tm_min));
	alloc_field(r,id_s,alloc_int(t->tm_sec));
	return r;
}

static value date_compare( value o, value d ) {
	int r;
	val_check_kind(o,k_date);
	val_check_kind(d,k_date);
	r = *val_date(o) - *val_date(d);
	return alloc_int( (r == 0)? 0 : ((r < 0) ? -1 : 1) );
}

static value date_sub( value o, value d ) {
	value r;
	val_check_kind(o,k_date);
	val_check_kind(d,k_date);
	r = alloc_abstract(k_date,alloc_private(sizeof(time_t)));	
	*val_date(r) = *val_date(o) - *val_date(d);
	return r;
}

static value set_locale(l) {
	if( !val_is_string(l) )
		return val_null;
	return alloc_bool(setlocale(LC_TIME,val_string(l)) != NULL);
}

DEFINE_PRIM(date_now,0);
DEFINE_PRIM(date_new,1);
DEFINE_PRIM(date_format,2);
DEFINE_PRIM(date_set_time,4);
DEFINE_PRIM(date_set_day,4);
DEFINE_PRIM(date_get_time,1);
DEFINE_PRIM(date_get_day,1);
DEFINE_PRIM(date_compare,2);
DEFINE_PRIM(date_sub,2);
DEFINE_PRIM(set_locale,1);

