#include <neko.h>
#include <stdlib.h>
typedef int SOCKET;
#include <mysql.h>

#define MYSQLDATA(o)	((MYSQL*)val_data(o))
#define RESULT(o)		((result*)val_data(o))

DEFINE_KIND(k_connection);
DEFINE_KIND(k_result);

static void error( MYSQL *m, const char *msg ) {
	buffer b = alloc_buffer(msg);
	buffer_append(b,mysql_error(m));
	val_throw( buffer_to_string(b) );
}

// ---------------------------------------------------------------
// Result

#undef CONV_FLOAT
typedef enum {
	CONV_INT,
	CONV_STRING,
	CONV_FLOAT
} CONV;

typedef struct {
	MYSQL_RES *r;
	int nfields;
	CONV *fields_convs;
	field *fields_ids;
	MYSQL_ROW current;
} result;

static field current_id;

static void free_result( value o ) {
	result *r = RESULT(o);
	mysql_free_result(r->r);
}

static value result_get_length( value o ) {
	if( val_is_int(o) )
		return o;
	val_check_kind(o,k_result);
	return alloc_int( (int)mysql_num_rows(RESULT(o)->r) );
}

static value result_get_nfields( value o ) {
	val_check_kind(o,k_result);
	return alloc_int(RESULT(o)->nfields);
}

static value result_next( value o ) {
	result *r;
	MYSQL_ROW row;
	val_check_kind(o,k_result);
	r = RESULT(o);
	row = mysql_fetch_row(r->r);
	if( row == NULL )
		return val_null;
	{
		int i;
		value cur = alloc_object(NULL);
		r->current = row;
		for(i=0;i<r->nfields;i++)
			if( row[i] != NULL ) {
				value v;
				switch( r->fields_convs[i] ) {
				case CONV_INT:
					v = alloc_int(atoi(row[i]));
					break;
				case CONV_STRING:
					v = alloc_string(row[i]);
					break;
				case CONV_FLOAT:
					v = alloc_float(atof(row[i]));
					break;
				default:
					v = val_null;
					break;
				}
				alloc_field(cur,r->fields_ids[i],v);
			}
		return cur;
	}
}

static value result_get( value o, value n ) {
	result *r;
	const char *s;
	val_check_kind(o,k_result);
	r = RESULT(o);
	if( !val_is_int(n) || val_int(n) < 0  || val_int(n) >= r->nfields )
		return NULL;
	if( !r->current ) {
		result_next(o);
		if( !r->current )
			return val_null;
	}
	s = r->current[val_int(n)];
	return alloc_string( s?s:"" );
}

static value result_get_int( value o, value n ) {
	value cur;
	result *r;
	const char *s;
	val_check_kind(o,k_result);
	r = RESULT(o);
	if( !val_is_int(n) || val_int(n) < 0  || val_int(n) >= r->nfields )
		return NULL;
	if( !r->current ) {
		result_next(o);
		if( !r->current )
			return val_null;
	}
	cur = val_field(o,current_id);
	s = r->current[val_int(n)];
	return alloc_int( s?atoi(s):0 );
}

static value result_get_float( value o, value n ) {
	result *r;
	const char *s;
	val_check_kind(o,k_result);
	r = RESULT(o);
	if( !val_is_int(n) || val_int(n) < 0  || val_int(n) >= r->nfields )
		return NULL;
	if( !r->current ) {
		result_next(o);
		if( !r->current )
			return val_null;
	}
	s = r->current[val_int(n)];
	return alloc_float( s?atof(s):0 );
}

static CONV convert_type( enum enum_field_types t ) {
	switch( t ) {
	case FIELD_TYPE_TINY:
	case FIELD_TYPE_LONG:
		return CONV_INT;
	case FIELD_TYPE_LONGLONG:
	case FIELD_TYPE_DECIMAL:
	case FIELD_TYPE_FLOAT:
	case FIELD_TYPE_DOUBLE:
		return CONV_FLOAT;
	default:
		return CONV_STRING;
	}
}

static value alloc_result( MYSQL_RES *r ) {
	result *res = (result*)alloc(sizeof(result));
	value o = alloc_abstract(k_result,res);
	int num_fields = mysql_num_fields(r);
	int i,j;
	MYSQL_FIELD *fields = mysql_fetch_fields(r);
	res->r = r;
	res->current = NULL;
	res->nfields = num_fields;
	res->fields_ids = (field*)alloc_private(sizeof(field)*num_fields);
	res->fields_convs = (CONV*)alloc_private(sizeof(CONV)*num_fields);
	for(i=0;i<num_fields;i++) {
		field id = val_id(fields[i].name);
		for(j=0;j<i;j++)
			if( res->fields_ids[j] == id ) {
				buffer b = alloc_buffer("Error, same field ids for : <b>");
				buffer_append(b,fields[i].name);
				buffer_append(b,":");
				val_buffer(b,alloc_int(i));
				buffer_append(b,"</b> and <b>");
				buffer_append(b,fields[j].name);
				buffer_append(b,":");
				val_buffer(b,alloc_int(j));
				buffer_append(b,"</b>.<br/>");
				val_throw(buffer_to_string(b));
			}
		res->fields_ids[i] = id;
		res->fields_convs[i] = convert_type(fields[i].type); 
	}
	val_gc(o,free_result);
	return o;
}

// ---------------------------------------------------------------
// Connection

static value close( value o ) {
	val_check_kind(o,k_connection);
	mysql_close(MYSQLDATA(o));
	val_data(o) = NULL;
	val_kind(o) = NULL;
	val_gc(o,NULL);
	return val_true;
}

static value selectDB( value o, value db ) {
	val_check_kind(o,k_connection);
	if( !val_is_string(db) )
		return val_null;
	if( mysql_select_db(MYSQLDATA(o),val_string(db)) != 0 ) {
		error(MYSQLDATA(o),"Failed to select database :");
		return val_false;
	}
	return val_true;
}

static value request( value o, value r )  {
	MYSQL_RES *res;
	val_check_kind(o,k_connection);
	if( !val_is_string(r) )
		return val_null;
	if( mysql_real_query(MYSQLDATA(o),val_string(r),val_strlen(r)) != 0 ) {
		error(MYSQLDATA(o),val_string(r));
		return val_null;
	}
	res = mysql_store_result(MYSQLDATA(o));
	if( res == NULL ) {
		if( mysql_field_count(MYSQLDATA(o)) == 0 )
			return alloc_int( (int)mysql_affected_rows(MYSQLDATA(o)) );
		else {
			error(MYSQLDATA(o),val_string(r));
			return val_null;
		}
	}	
	return alloc_result(res);
}

// ---------------------------------------------------------------
// Sql


static void free_connection( value o ) {
	mysql_close(MYSQLDATA(o));
}

static value connect( value params  ) {
	value host, port, user, pass, socket;
	if( !val_is_object(params) )
		return val_null;
	host = val_field(params,val_id("host"));
	port = val_field(params,val_id("port"));
	user = val_field(params,val_id("user"));
	pass = val_field(params,val_id("pass"));
	socket = val_field(params,val_id("socket"));
	if( !(
		val_is_string(host) &&
		val_is_int(port) &&
		val_is_string(user) &&
		val_is_string(pass) &&
		(val_is_string(socket) || val_is_null(socket))
	) )
		return val_null;
	{
		MYSQL *m = mysql_init(NULL);
		value v;
		if( mysql_real_connect(m,val_string(host),val_string(user),val_string(pass),NULL,val_int(port),val_is_null(socket)?NULL:val_string(socket),0) == NULL ) {
			error(m,"Failed to connect to mysql server :");
			mysql_close(m);
			return val_null;
		}
		v = alloc_abstract(k_connection,m);
		val_gc(v,free_connection);
		return v;
	}	
}

// ---------------------------------------------------------------
// Registers

static void init() {
	current_id = val_id("current");
}

DEFINE_ENTRY_POINT(init);

DEFINE_PRIM(connect,1);
DEFINE_PRIM(close,1);
DEFINE_PRIM(request,2);
DEFINE_PRIM(selectDB,2);

DEFINE_PRIM(result_get_length,1);
DEFINE_PRIM(result_get_nfields,1);
DEFINE_PRIM(result_next,1);
DEFINE_PRIM(result_get,2);
DEFINE_PRIM(result_get_int,2);
DEFINE_PRIM(result_get_float,2);
