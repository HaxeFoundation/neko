/* ************************************************************************ */
/*																			*/
/*  Neko Sqlite bindings Library											*/
/*  Copyright (c)2006 Motion-Twin											*/
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

/**
	<doc>
	<h1>SQLite</h1>
	<p>
	Sqlite is a small embeddable SQL database that store all its data into
	a single file. See http://sqlite.org for more details.
	</p>
	</doc>
**/

DEFINE_KIND(k_db);
DEFINE_KIND(k_result);

#define val_db(v)	((database*)val_data(v))
#define val_result(v) ((result*)val_data(v))

typedef struct _database {
	sqlite3 *db;
	value last;
} database;

typedef struct _result {
	database *db;
	int ncols;
	int count;
	field *names;
	int *bools;
	int done;
	int first;
	sqlite3_stmt *r;
} result;

static void sqlite_error( sqlite3 *db ) {
	buffer b = alloc_buffer("Sqlite error : ");
	buffer_append(b,sqlite3_errmsg(db));
	val_throw(buffer_to_string(b));
}

static void finalize_result( result *r, int exc ) {
	r->first = 0;
	r->done = 1;
	if( r->ncols == 0 )
		r->count = sqlite3_changes(r->db->db);
	if( sqlite3_finalize(r->r) != SQLITE_OK && exc )
		val_throw(alloc_string("Could not finalize request"));
	r->r = NULL;
	r->db->last = NULL;
	r->db = NULL;
}

static void free_db( value v ) {
	database *db = val_db(v);
	if( db->last != NULL )
		finalize_result(val_result(db->last),0);
	if( sqlite3_close(db->db) != SQLITE_OK )
		sqlite_error(db->db);
}

/**
	connect : filename:string -> 'db
	<doc>Open or create the database stored in the specified file.</doc>
**/
static value connect( value filename ) {
	int err;
	database *db = (database*)alloc(sizeof(database));
	value v;
	val_check(filename,string);
	db->last = NULL;
	if( (err = sqlite3_open(val_string(filename),&db->db)) != SQLITE_OK ) {
		buffer b = alloc_buffer("Sqlite error : ");
		buffer_append(b,sqlite3_errmsg(db->db));
		sqlite3_close(db->db);
		val_throw(buffer_to_string(b));
	}
	v = alloc_abstract(k_db,db);
	val_gc(v,free_db);
	return v;
}

/**
	close : 'db -> void
	<doc>Closes the database.</doc>
**/
static value close( value v ) {
	val_check_kind(v,k_db);
	free_db(v);
	val_gc(v,NULL);
	val_kind(v) = NULL;
	return val_null;
}

/**
	last_insert_id : 'db -> int
	<doc>Returns the last inserted auto_increment id.</doc>
**/
static value last_insert_id( value db ) {
	val_check_kind(db,k_db);
	return alloc_int(sqlite3_last_insert_rowid(val_db(db)->db));
}

/**
	request : 'db -> sql:string -> 'result
	<doc>Executes the SQL request and returns its result</doc>
**/
static value request( value v, value sql ) {
	database *db;
	result *r;
	const char *tl;
	int i,j;
	val_check_kind(v,k_db);
	val_check(sql,string);
	db = val_db(v);
	r = (result*)alloc(sizeof(result));
	r->db = db;
	if( sqlite3_prepare(db->db,val_string(sql),val_strlen(sql),&r->r,&tl) != SQLITE_OK ) {
		buffer b = alloc_buffer("Sqlite error in ");
		val_buffer(b,sql);
		buffer_append(b," : ");
		buffer_append(b,sqlite3_errmsg(db->db));
		val_throw(buffer_to_string(b));
	}
	if( *tl ) {
		sqlite3_finalize(r->r);
		val_throw(alloc_string("Cannot execute several SQL requests at the same time"));
	}
	r->ncols = sqlite3_column_count(r->r);
	r->names = (field*)alloc(sizeof(field)*r->ncols);
	r->bools = (int*)alloc(sizeof(int)*r->ncols);
	r->first = 1;
	r->done = 0;
	for(i=0;i<r->ncols;i++) {
		field id = val_id(sqlite3_column_name(r->r,i));
		const char *dtype = sqlite3_column_decltype(r->r,i);
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
		r->bools[i] = dtype?(strcmp(dtype,"BOOL") == 0):0;
	}
	// changes in an update/delete
	if( db->last != NULL )
		finalize_result(val_result(db->last),0);
	db->last = alloc_abstract(k_result,r);
	return db->last;
}

/**
	result_get_length : 'result -> int
	<doc>Returns the number of rows in the result or the number of rows changed by the request.</doc>
**/
static value result_get_length( value v ) {
	result *r;
	val_check_kind(v,k_result);
	r = val_result(v);
	if( r->ncols != 0 )
		neko_error(); // ???
	return alloc_int(r->count);
}

/**
	result_get_nfields : 'result -> int
	<doc>Returns the number of fields in the result.</doc>
**/
static value result_get_nfields( value r ) {
	val_check_kind(r,k_result);
	return alloc_int(val_result(r)->ncols);
}

/**
	result_next : 'result -> object?
	<doc>Returns the next row in the result or [null] if no more result.</doc>
**/
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
				if( r->bools[i] )
					f = alloc_bool(sqlite3_column_int(r->r,i));
				else
					f = alloc_int(sqlite3_column_int(r->r,i));
				break;
			case SQLITE_FLOAT:
				f = alloc_float(sqlite3_column_double(r->r,i));
				break;
			case SQLITE_TEXT:
				f = alloc_string((char*)sqlite3_column_text(r->r,i));
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
		finalize_result(r,1);
		return val_null;
	case SQLITE_BUSY:
		val_throw(alloc_string("Database is busy"));
	case SQLITE_ERROR:
		sqlite_error(r->db->db);
	default:
		neko_error();
	}
	return val_null;
}

/**
	result_get : 'result -> n:int -> string
	<doc>Return the [n]th field of the current result row.</doc>
**/
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
	return alloc_string((char*)sqlite3_column_text(r->r,val_int(n)));
}

/**
	result_get_int : 'result -> n:int -> int
	<doc>Return the [n]th field of the current result row as an integer.</doc>
**/
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

/**
	result_get_float : 'result -> n:int -> float
	<doc>Return the [n]th field of the current result row as a float.</doc>
**/
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
DEFINE_PRIM(last_insert_id,1);

DEFINE_PRIM(result_get_length,1);
DEFINE_PRIM(result_get_nfields,1);

DEFINE_PRIM(result_next,1);
DEFINE_PRIM(result_get,2);
DEFINE_PRIM(result_get_int,2);
DEFINE_PRIM(result_get_float,2);

/* ************************************************************************ */
