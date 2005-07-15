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
/*
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
	free(r->fields_ids);
	free(r->fields_convs);
	free(r);
}

static value result_toString() {
	value o = val_this();
	val_check_obj(o,t_result);
	return alloc_string("#Sql.Result");
}

static value empty_result_toString() {
	return alloc_string("#Sql.EmptyResult");
}

static value empty_result_get_length() {
	return val_field(val_this(),val_id("_n"));
}

static value result_get_length() {
	value o = val_this();
	val_check_obj(o,t_result);
	return alloc_int( (int)mysql_num_rows(RESULT(o)->r) );
}

static value result_get_nfields() {
	value o = val_this();
	val_check_obj(o,t_result);
	return alloc_int(RESULT(o)->nfields);
}

static value result_next() {
	value o = val_this();
	result *r;
	MYSQL_ROW row;
	val_check_obj(o,t_result);
	r = RESULT(o);
	row = mysql_fetch_row(r->r);
	if( row == NULL )
		return val_false;
	{
		int i;
		value cur = alloc_object(NULL);
		alloc_field(o,current_id,cur);
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
		return val_true;
	}
}

static value result_getResult( value n ) {
	value o = val_this();
	result *r;
	const char *s;
	val_check_obj(o,t_result);
	r = RESULT(o);
	if( !val_is_int(n) || val_int(n) < 0  || val_int(n) >= r->nfields )
		return NULL;
	if( !r->current ) {
		result_next();
		if( !r->current )
			return val_null;
	}
	s = r->current[val_int(n)];
	return alloc_string( s?s:"" );
}

static value result_getIntResult( value n ) {
	value o = val_this();
	value cur;
	result *r;
	const char *s;
	val_check_obj(o,t_result);
	r = RESULT(o);
	if( !val_is_int(n) || val_int(n) < 0  || val_int(n) >= r->nfields )
		return NULL;
	if( !r->current ) {
		result_next();
		if( !r->current )
			return val_null;
	}
	cur = val_field(o,current_id);
	s = r->current[val_int(n)];
	return alloc_int( s?atoi(s):0 );
}

static value result_getFloatResult( value n ) {
	value o = val_this();
	result *r;
	const char *s;
	val_check_obj(o,t_result);
	r = RESULT(o);
	if( !val_is_int(n) || val_int(n) < 0  || val_int(n) >= r->nfields )
		return NULL;
	if( !r->current ) {
		result_next();
		if( !r->current )
			return val_null;
	}
	s = r->current[val_int(n)];
	return alloc_float( s?atof(s):0 );
}

static value std_mysql_result() {
	value o = alloc_class(&t_result);
	Method(o,result,get_nfields,0);
	Method(o,result,toString,0);
	Method(o,result,get_length,0);
	Method(o,result,getResult,1);
	Method(o,result,getIntResult,1);
	Method(o,result,getFloatResult,1);
	Method(o,result,next,0);
	current_id = val_id("current");	
	return o;
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
	value o = alloc_object(&t_result);
	int num_fields = mysql_num_fields(r);
	int i,j;
	MYSQL_FIELD *fields = mysql_fetch_fields(r);
	result *res = (result*)alloc(sizeof(result));
	res->r = r;
	res->current = NULL;
	res->nfields = num_fields;
	res->fields_ids = (field*)alloc_abstract(sizeof(field)*num_fields);
	res->fields_convs = (CONV*)alloc_abstract(sizeof(CONV)*num_fields);
	for(i=0;i<num_fields;i++) {
		field id = val_id(fields[i].name);
		for(j=0;j<i;j++)
			if( res->fields_ids[j] == id ) {
				val_print(alloc_string("Error, same field ids for : <b>"));
				val_print(alloc_string(fields[i].name));
				val_print(alloc_string(":"));
				val_print(alloc_int(i));
				val_print(alloc_string("</b> and <b>"));
				val_print(alloc_string(fields[j].name));
				val_print(alloc_string(":"));
				val_print(alloc_int(j));
				val_print(alloc_string("</b>.<br/>"));
			}
		res->fields_ids[i] = id;
		res->fields_convs[i] = convert_type(fields[i].type); 
	}
	val_odata(o) = (value)res;
	val_gc(o,free_result);
	return o;
}

// ---------------------------------------------------------------
// Connection

static value connection_close() {
	value o = val_this();
	val_check_obj(o,t_connection);
	mysql_close(MYSQLDATA(o));
	val_odata(o) = NULL;
	val_otype(o) = NULL;
	val_gc(o,NULL);
	return val_true;
}

static value connection_selectDB( value db ) {
	value o = val_this();
	val_check_obj(o,t_connection);
	if( !val_is_string(db) )
		return NULL;
	if( mysql_select_db(MYSQLDATA(o),val_string(db)) != 0 ) {
		error(MYSQLDATA(o),"Failed to select database :");
		return val_false;
	}
	return val_true;
}

static value connection_request( value r )  {
	value o = val_this();
	MYSQL_RES *res;
	val_check_obj(o,t_connection);
	if( !val_is_string(r) )
		return NULL;
	if( mysql_real_query(MYSQLDATA(o),val_string(r),val_strlen(r)) != 0 ) {
		error(MYSQLDATA(o),val_string(r));
		return val_null;
	}
	res = mysql_store_result(MYSQLDATA(o));
	if( res == NULL ) {
		if( mysql_field_count(MYSQLDATA(o)) == 0 ) {
			// no result : insert for example
			int n = (int)mysql_affected_rows(MYSQLDATA(o));
			o = alloc_object(NULL);
			alloc_field(o,val_id("_n"),alloc_int(n));
			Method(o,empty_result,toString,0);
			Method(o,empty_result,get_length,0);
			return o;
		} else {
			error(MYSQLDATA(o),val_string(r));
			return val_null;
		}
	}
	return alloc_result(res);
}

value std_mysql_connection() {
	value o = alloc_class(&t_connection);
	Method(o,connection,selectDB,1);
	Method(o,connection,request,1);
	Method(o,connection,close,0);
	return o;
}

*/
// ---------------------------------------------------------------
// Sql


static void free_connection( value o ) {
	mysql_close(MYSQLDATA(o));
}

value connect( value params  ) {
	value host, port, user, pass, socket;
	if( !val_is_object(params) )
		return NULL;
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
		return NULL;
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

DEFINE_PRIM(connect,1);
