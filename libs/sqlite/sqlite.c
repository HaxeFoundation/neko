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
#include <sqlite3.h>

DEFINE_KIND(k_db);
DEFINE_KIND(k_result);

#define val_db(v)	((sqlite3*)val_data(v))
#define val_result(v) ((result*)val_data(v))

typedef struct _result {
	value db;
	int ncols;
	int count;
	field *names;	
	int done;
	int first;
	sqlite3_stmt *r;
} result;

static void sqlite_error( sqlite3 *db ) {
	buffer b = alloc_buffer("Sqlite error : ");
	buffer_append(b,sqlite3_errmsg(db));
	val_throw(buffer_to_string(b));
}

static void free_db( value v ) {
	sqlite3_close(val_db(v));
}

static void free_result( value v ) {
	sqlite3_finalize(val_result(v)->r);
}

static value connect( value filename ) {
	int err;
	sqlite3 *db;
	value v;
	val_check(filename,string);
	if( (err = sqlite3_open(val_string(filename),&db)) != SQLITE_OK ) {		
		buffer b = alloc_buffer("Sqlite error : ");
		buffer_append(b,sqlite3_errmsg(db));
		sqlite3_close(db);
		val_throw(buffer_to_string(b));
	}
	v = alloc_abstract(k_db,db);
	val_gc(v,free_db);
	return v;
}

static value close( value db ) {
	val_check_kind(db,k_db);
	if( sqlite3_close(val_db(db)) != SQLITE_OK )
		sqlite_error(val_db(db));
	val_gc(db,NULL);
	val_kind(db) = NULL;
	return val_null;
}

static value request( value db, value sql ) {
	result *r;
	const char *tl;
	value v;
	int i,j;
	val_check_kind(db,k_db);
	val_check(sql,string);
	r = (result*)alloc(sizeof(result));
	r->db = db;
	if( sqlite3_prepare(val_db(db),val_string(sql),val_strlen(sql),&r->r,&tl) != SQLITE_OK )
		sqlite_error(val_db(db));
	r->ncols = sqlite3_column_count(r->r);
	r->names = (field*)alloc(sizeof(field)*r->ncols);
	r->first = 1;
	r->done = 0;
	for(i=0;i<r->ncols;i++) {
		field id = val_id(sqlite3_column_name(r->r,i));
		for(j=0;j<i;j++)
			if( r->names[j] == id ) {
				if( strcmp(sqlite3_column_name(r->r,i),sqlite3_column_name(r->r,j)) == 0 ) {
					buffer b = alloc_buffer("Error, same field is two times in the request ");
					val_buffer(b,sql);
					sqlite3_finalize(r->r);
					val_throw(buffer_to_string(b));
				} else {
					buffer b = alloc_buffer("Error, same field ids for : ");
					buffer_append(b,sqlite3_column_name(r->r,i));
					buffer_append(b," and ");
					buffer_append(b,sqlite3_column_name(r->r,j));
					buffer_append_char(b,'.');
					sqlite3_finalize(r->r);
					val_throw(buffer_to_string(b));
				}
			}
		r->names[i] = id;
	}
	// changes in an update/delete
	v = alloc_abstract(k_result,r);
	val_gc(v,free_result);
	return v;
}

static value result_get_length( value v ) {
	result *r;
	val_check_kind(v,k_result);
	r = val_result(v);
	if( r->ncols != 0 )
		neko_error(); // ???
	return alloc_int(val_db(r->count));
}

static value result_get_nfields( value r ) {
	val_check_kind(r,k_result);
	return alloc_int(val_result(r)->ncols);
}

static value result_next( value v ) {
	int i;
	result *r;
	val_check_kind(v,k_result);
	r = val_result(v);
	if( r->done )
		return val_null;	
	switch( sqlite3_step(r->r) ) {
	case SQLITE_ROW:
		r->first = 0;
		v = alloc_object(NULL);
		for(i=0;i<r->ncols;i++) {
			value f;
			switch( sqlite3_column_type(r->r,i) ) {
			case SQLITE_NULL:
				f = val_null;
				break;
			case SQLITE_INTEGER:
				f = alloc_int(sqlite3_column_int(r->r,i));
				break;
			case SQLITE_FLOAT:
				f = alloc_float(sqlite3_column_double(r->r,i));
				break;
			case SQLITE_TEXT:
				f = alloc_string(sqlite3_column_text(r->r,i));
				break;
			case SQLITE_BLOB:
				{
					int size = sqlite3_column_bytes(r->r,i);
					f = alloc_empty_string(size);
					memcpy(val_string(f),sqlite3_column_blob(r->r,i),size);
					break;
				}
			default:
				{
					buffer b = alloc_buffer("Unknown Sqlite type #");
					val_buffer(b,alloc_int(sqlite3_column_type(r->r,i)));
					val_throw(buffer_to_string(b));
				}
			}
			alloc_field(v,r->names[i],f);
		}
		return v;
	case SQLITE_DONE:
		r->done = 1;
		r->first = 0;
		if( r->ncols == 0 )
			r->count = sqlite3_changes(val_db(r->db));
		return val_null;
	case SQLITE_BUSY:
		val_throw(alloc_string("Database is busy"));
	case SQLITE_ERROR:
		sqlite_error(val_db(r->db));
	default:
		neko_error();
	}
	return val_null;
}

static value result_get( value v, value n ) {
	result *r;
	val_check_kind(v,k_result);
	r = val_result(v);
	if( val_int(n) < 0 || val_int(n) >= r->ncols )
		neko_error();
	if( r->first )
		result_next(v);
	if( r->done )
		neko_error();
	return alloc_string(sqlite3_column_text(r->r,val_int(n)));
}

static value result_get_int( value v, value n ) {
	result *r;
	val_check_kind(v,k_result);
	r = val_result(v);
	if( val_int(n) < 0 || val_int(n) >= r->ncols )
		neko_error();
	if( r->first )
		result_next(v);
	if( r->done )
		neko_error();
	return alloc_int(sqlite3_column_int(r->r,val_int(n)));
}

static value result_get_float( value v, value n ) {
	result *r;
	val_check_kind(v,k_result);
	r = val_result(v);
	if( val_int(n) < 0 || val_int(n) >= r->ncols )
		neko_error();
	if( r->first )
		result_next(v);
	if( r->done )
		neko_error();
	return alloc_float(sqlite3_column_double(r->r,val_int(n)));
}

DEFINE_PRIM(connect,1);
DEFINE_PRIM(close,1);
DEFINE_PRIM(request,2);

DEFINE_PRIM(result_get_length,1);
DEFINE_PRIM(result_get_nfields,1);

DEFINE_PRIM(result_next,1);
DEFINE_PRIM(result_get,2);
DEFINE_PRIM(result_get_int,2);
DEFINE_PRIM(result_get_float,2);

/* ************************************************************************ */
