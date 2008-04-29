/* ************************************************************************ */
/*																			*/
/*  Neko Apache Library														*/
/*  Copyright (c)2005 Motion-Twin											*/
/*																			*/
/* This library is free software; you can redistribute it and/or			*/
/* modify it under the terms of the GNU Lesser General Public				*/
/* License as published by the Free Software Foundation; either				*/
/* version 2.1 of the License, or (at your option) any later version.		*/
/*																			*/
/* This library is distributed in the hope that it will be useful,			*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU		*/
/* Lesser General Public License or the LICENSE file for more details.		*/
/*																			*/
/* ************************************************************************ */
#include <neko.h>
#include <string.h>
#include "mod_neko.h"

DEFINE_KIND(k_mod_neko);

#ifndef NEKO_WINDOWS
#	define strcmpi	strcasecmp
#endif

#ifdef APACHE_2_X
#	define ap_table_get		apr_table_get
#	define ap_table_set		apr_table_set
#	define ap_table_add		apr_table_add
#	define ap_table_do		apr_table_do
#	define REDIRECT			HTTP_MOVED_TEMPORARILY
#endif

#define PARSE_HEADER(start,cursor) \
	cursor = start; \
	if( *cursor == '"' ) { \
		start++; \
		cursor++; \
		while( *cursor != '"' && *cursor != 0 ) \
			cursor++; \
	} else { \
		while( *cursor != 0 && *cursor != '\r' && *cursor != '\n' && *cursor != '\t' ) \
			cursor++; \
	}

#define HEADERS_NOT_SENT(msg) \
	if( c->headers_sent ) { \
		buffer b = alloc_buffer("Cannot set "); \
		buffer_append(b,msg); \
		buffer_append(b," : Headers already sent"); \
		bfailure(b); \
	}

/**
	<doc>
	<h1>Mod_neko</h1>
	<p>
	Apache access when running inside mod_neko.
	</p>
	</doc>
**/

/**
	get_cookies : void -> #list
	<doc>Return a cookie list as a (name,value) chained list</doc>
**/
static value get_cookies() {
	const char *k = ap_table_get(CONTEXT()->r->headers_in,"Cookie");
	char *start, *end;
	value p = val_null, tmp;
	if( k == NULL )
		return p;
	while( (start = strchr(k,'=')) != NULL ) {
		start++;
		end = start;
		while( *end != 0 && *end != '\r' && *end != '\n' && *end != ';' )
			end++;
		tmp = alloc_array(3);
		val_array_ptr(tmp)[0] = copy_string(k,(int)(start-k-1));
		val_array_ptr(tmp)[1] = copy_string(start,(int)(end-start));
		val_array_ptr(tmp)[2] = p;
		p = tmp;
		if( *end != ';' || end[1] != ' ' )
			break;
		k = end + 2;
	}
	return p;
}

/**
	set_cookie : name:string -> val:string -> void
	<doc>Set a cookie</doc>
**/
static value set_cookie( value name, value v ) {
	mcontext *c = CONTEXT();
	buffer b;
	value str;
	val_check(name,string);
	val_check(v,string);
	HEADERS_NOT_SENT("Cookie");
	b = alloc_buffer(NULL);
	val_buffer(b,name);
	buffer_append(b,"=");
	val_buffer(b,v);
	buffer_append(b,";");
	str = buffer_to_string(b);
	ap_table_add(c->r->headers_out,"Set-Cookie",val_string(str));
	return val_true;
}

/**
	get_host_name : void -> string
	<doc>Get the local host IP</doc>
**/
static value get_host_name() {
	mcontext *c = CONTEXT();
	const char *h = c->r->connection->local_host;
	return alloc_string( h?h:c->r->connection->local_ip );
}

/**
	get_client_ip : void -> string
	<doc>Get the connected client IP</doc>
**/
static value get_client_ip() {
	return alloc_string( CONTEXT()->r->connection->remote_ip );
}

/**
	get_uri : void -> string
	<doc>Get the original URI requested by the client (before any internal redirection)</doc>
**/
static value get_uri() {
	request_rec *r = CONTEXT()->r;
	while( r->prev != NULL )
		r = r->prev;
	return alloc_string( r->uri );
}

/**
	redirect : string -> void
	<doc>Redirect the client to another page (Location header)</doc>
**/
static value redirect( value s ) {
	mcontext *c = CONTEXT();
	val_check(s,string);
	HEADERS_NOT_SENT("Redirection");
	ap_table_set(c->r->headers_out,"Location",val_string(s));
	c->r->status = REDIRECT;
	return val_true;
}

/**
	set_return_code : int -> void
	<doc>Set the HTTP return code</doc>
**/
static value set_return_code( value i ) {
	mcontext *c = CONTEXT();
	val_check(i,int);
	HEADERS_NOT_SENT("Return code");
	c->r->status = val_int(i);
	return val_true;
}

/**
	set_header : name:string -> val:string -> void
	<doc>Set a HTTP header value</doc>
**/
static value set_header( value s, value k ) {
	mcontext *c = CONTEXT();
	val_check(s,string);
	val_check(k,string);
	HEADERS_NOT_SENT("Header");
	if( strcmpi(val_string(s),"Content-Type") == 0 ) {
		c->content_type = alloc_string(val_string(k));
		c->r->content_type = val_string(c->content_type);
	} else
		ap_table_set(c->r->headers_out,val_string(s),val_string(k));
	return val_true;
}

/**
	get_client_header : name:string -> string?
	<doc>Get a HTTP header sent by the client</doc>
**/
static value get_client_header( value s ) {
	mcontext *c = CONTEXT();
	val_check(s,string);
	return alloc_string( ap_table_get(c->r->headers_in,val_string(s)) );
}


static int store_table( void *r, const char *key, const char *val ) {
	value a;
	if( key == NULL || val == NULL )
		return 1;
	a = alloc_array(2);
	a = alloc_array(3);
	val_array_ptr(a)[0] = alloc_string(key);
	val_array_ptr(a)[1] = alloc_string(val);
	val_array_ptr(a)[2] = *(value*)r;
	*((value*)r) = a;
	return 1;
}
/**
	get_client_headers : void -> string list
	<doc>Get all the HTTP client headers</doc>
**/
static value get_client_headers() {
	value r = val_null;
	ap_table_do(store_table,&r,CONTEXT()->r->headers_in,NULL);
	return r;
}

/**
	get_params_string : void -> string
	<doc>Return the whole parameters string</doc>
**/
static value get_params_string() {
	return alloc_string(CONTEXT()->r->args);
}

/**
	get_post_data : void -> string
	<doc>Return the whole unparsed POST string</doc>
**/
static value get_post_data() {
	return CONTEXT()->post_data;
}

static char *memfind( char *mem, int mlen, const char *v ) {
	char *found;
	int len = (int)strlen(v);
	if( len == 0 )
		return mem;
	while( (found = memchr(mem,*v,mlen)) != NULL ) {
		if( (int)(found - mem) + len > mlen )
			break;
		if( memcmp(found,v,len) == 0 )
			return found;
		mlen -= (int)(found - mem + 1);
		mem = found + 1;
	}
	return NULL;
}

#define BUFSIZE 1024

static void fill_buffer( mcontext *c, value buf, int *len ) {
	int pos = *len;
	while( pos < BUFSIZE ) {
		int k = ap_get_client_block(c->r,val_string(buf)+pos,BUFSIZE-pos);
		if( k == 0 )
			break;
		pos += k;
	}
	*len = pos;
}

static value discard_body( mcontext *c ) {
	char buf[256];
	while( ap_get_client_block(c->r,buf,256) > 0 ) {
	}
	neko_error();
}

/**
	parse_multipart_data : onpart:function:2 -> ondata:function:3 -> void
	<doc>
	Incrementally parse the multipart data. call [onpart(name,filename)] for each part
	found and [ondata(buf,pos,len)] when some data is available
	</doc>
**/
static value parse_multipart_data( value onpart, value ondata ) {
	value buf;
	int len = 0;
	mcontext *c = CONTEXT();
	const char *ctype = ap_table_get(c->r->headers_in,"Content-Type");
	value boundstr;
	val_check_function(onpart,2);
	val_check_function(ondata,3);
	buf = alloc_empty_string(BUFSIZE);
	if( !ctype || strstr(ctype,"multipart/form-data") == NULL )
		return val_null;
	// extract boundary value
	{
		const char *boundary, *bend;
		if( (boundary = strstr(ctype,"boundary=")) == NULL )
			neko_error();
		boundary += 9;
		PARSE_HEADER(boundary,bend);
		len = (int)(bend - boundary);
		boundstr = alloc_empty_string(len+2);
		if( val_strlen(boundstr) > BUFSIZE / 2 )
			neko_error();
		val_string(boundstr)[0] = '-';
		val_string(boundstr)[1] = '-';
		memcpy(val_string(boundstr)+2,boundary,len);
	}
	len = 0;
    if( !ap_should_client_block(c->r) )
		neko_error();
	while( true ) {
		char *name, *end_name, *filename, *end_file_name, *data;
		int pos;
		// refill buffer
		// we assume here that the the whole multipart header can fit in the buffer
		fill_buffer(c,buf,&len);
		// is boundary at the beginning of buffer ?
		if( len < val_strlen(boundstr) || memcmp(val_string(buf),val_string(boundstr),val_strlen(boundstr)) != 0 )
			return discard_body(c);
		name = memfind(val_string(buf),len,"Content-Disposition:");
		if( name == NULL )
			break;
		name = memfind(name,len - (int)(name - val_string(buf)),"name=");
		if( name == NULL )
			return discard_body(c);
		name += 5;
		PARSE_HEADER(name,end_name);
		data = memfind(end_name,len - (int)(end_name - val_string(buf)),"\r\n\r\n");
		if( data == NULL )
			return discard_body(c);
		filename = memfind(name,(int)(data - name),"filename=");
		if( filename != NULL ) {
			filename += 9;
			PARSE_HEADER(filename,end_file_name);
		}
		data += 4;
		pos = (int)(data - val_string(buf));
		// send part name
		val_call2(onpart,copy_string(name,(int)(end_name - name)),filename?copy_string(filename,(int)(end_file_name - filename)):val_null);
		// read data
		while( true ) {
			const char *boundary;
			// recall buffer
			memcpy(val_string(buf),val_string(buf)+pos,len - pos);
			len -= pos;
			pos = 0;
			fill_buffer(c,buf,&len);
			// lookup bounds
			boundary = memfind(val_string(buf),len,val_string(boundstr));
			if( boundary == NULL ) {
				if( len == 0 )
					return discard_body(c);
				// send as much buffer as possible to client
				if( len < BUFSIZE )
					pos = len;
				else
					pos = len - val_strlen(boundstr) + 1;
				val_call3(ondata,buf,alloc_int(0),alloc_int(pos));
			} else {
				// send remaining data
				pos = (int)(boundary - val_string(buf));
				val_call3(ondata,buf,alloc_int(0),alloc_int(pos-2));
				// recall
				memcpy(val_string(buf),val_string(buf)+pos,len - pos);
				len -= pos;
				break;
			}
		}
	}
	return val_null;
}

static value url_decode( const char *in, int len ) {
	int pin = 0;
	int pout = 0;
	value v = alloc_empty_string(len);
	char *out = (char*)val_string(v);
	while( len-- > 0 ) {
		char c = in[pin++];
		if( c == '+' )
			c = ' ';
		else if( c == '%' ) {
			int p1, p2;
			if( len < 2 )
				break;
			p1 = in[pin++];
			p2 = in[pin++];
			len -= 2;
			if( p1 >= '0' && p1 <= '9' )
				p1 -= '0';
			else if( p1 >= 'a' && p1 <= 'f' )
				p1 -= 'a' - 10;
			else if( p1 >= 'A' && p1 <= 'F' )
				p1 -= 'A' - 10;
			else
				continue;
			if( p2 >= '0' && p2 <= '9' )
				p2 -= '0';
			else if( p2 >= 'a' && p2 <= 'f' )
				p2 -= 'a' - 10;
			else if( p2 >= 'A' && p2 <= 'F' )
				p2 -= 'A' - 10;
			else
				continue;
			c = (char)((unsigned char)((p1 << 4) + p2));
		}
		out[pout++] = c;
	}
	out[pout] = 0;
	val_set_size(v,pout);
	return v;
}

static void parse_get( value *p, const char *args ) {
	char *aand, *aeq, *asep;
	value tmp;
	while( true ) {
		aand = strchr(args,'&');
		if( aand == NULL ) {
			asep = strchr(args,';');
			aand = asep;
		} else {
			asep = strchr(args,';');
			if( asep != NULL && asep < aand )
				aand = asep;
		}
		if( aand != NULL )
			*aand = 0;
		aeq = strchr(args,'=');
		if( aeq != NULL ) {
			*aeq = 0;
			tmp = alloc_array(3);
			val_array_ptr(tmp)[0] = url_decode(args,(int)(aeq-args));
			val_array_ptr(tmp)[1] = url_decode(aeq+1,(int)strlen(aeq+1));
			val_array_ptr(tmp)[2] = *p;
			*p = tmp;
			*aeq = '=';
		}
		if( aand == NULL )
			break;
		*aand = (aand == asep)?';':'&';
		args = aand+1;
	}
}

/**
	get_params : void -> #list
	<doc>parse all GET and POST params and return them into a chained list</doc>
**/
static value get_params() {
	mcontext *c = CONTEXT();
	const char *args = c->r->args;
	value p = val_null;

	// PARSE "GET" PARAMS
	if( args != NULL )
		parse_get(&p,args);

	// PARSE "POST" PARAMS
	if( c->post_data != NULL )
		parse_get(&p,val_string(c->post_data));

	return p;
}

/**
	cgi_get_cwd : void -> string
	<doc>Return current bytecode file working directory</doc>
**/
static value cgi_get_cwd() {
	mcontext *c = CONTEXT();
	char *s = strrchr(c->r->filename,'/');
	value v;
	char old;
	if( s != NULL ) {
		old = s[1];
		s[1] = 0;
	}
	v = alloc_string(c->r->filename);
	if( s != NULL )
		s[1] = old;
	return v;
}

/**
	cgi_set_main : ?function:0 -> void
	<doc>Set or disable the main entry point function</doc>
**/
static value cgi_set_main( value f ) {
	if( val_is_null(f) ) {
		CONTEXT()->main = NULL;
		return val_true;
	}
	val_check_function(f,0);
	CONTEXT()->main = f;
	return val_true;
}

/**
	cgi_flush : void -> void
	<doc>Flush the data written so it's immediatly sent to the client</doc>
**/
static value cgi_flush() {
	ap_rflush(CONTEXT()->r);
	return val_null;
}

/**
	cgi_get_config : void -> object
	<doc>Return the current configuration</doc>
**/
#define FSET(name,t)	alloc_field(v,val_id(#name),alloc_##t(c-> name))
static value cgi_get_config() {
	value v = alloc_object(NULL);
	mconfig *c = mod_neko_get_config();
	FSET(hits,int);
	FSET(use_jit,bool);
	FSET(use_stats,bool);
	FSET(use_prim_stats,bool);
	FSET(use_cache,bool);
	FSET(exceptions,int);
	FSET(gc_period,int);
	FSET(max_post_size,int);
	return v;
}

/**
	cgi_set_config : object -> void
	<doc>Set the current configuration</doc>
**/
#define FGET(name,t)	f = val_field(v,val_id(#name)); val_check(f,t); c. name = val_##t(f)
static value cgi_set_config( value v ) {
	mconfig c;
	value f;
	val_check(v,object);
	FGET(hits,int);
	FGET(use_jit,bool);
	FGET(use_stats,bool);
	FGET(use_prim_stats,bool);
	FGET(use_cache,bool);
	FGET(exceptions,int);
	FGET(gc_period,int);
	FGET(max_post_size,int);
	mod_neko_set_config(&c);
	return val_null;
}

/**
	cgi_command : any -> any
	<doc>Perform a configuration-specific command :
		<ul>
		<li>stats : returns the statistics</li>
		<li>cache : returns the current cache</li>
		</ul>
	</doc>
**/
extern value cgi_command( value v );

/**
	get_http_method : void -> string
	<doc>Returns the http method (GET,POST...) used by the client</doc>
**/
static value get_http_method() {
	return alloc_string(CONTEXT()->r->method);
}

DEFINE_PRIM(cgi_get_cwd,0);
DEFINE_PRIM(cgi_set_main,1);
DEFINE_PRIM(get_cookies,0);
DEFINE_PRIM(set_cookie,2);
DEFINE_PRIM(get_host_name,0);
DEFINE_PRIM(get_client_ip,0);
DEFINE_PRIM(get_uri,0);
DEFINE_PRIM(redirect,1);
DEFINE_PRIM(get_params,0);
DEFINE_PRIM(get_params_string,0);
DEFINE_PRIM(get_post_data,0);
DEFINE_PRIM(set_header,2);
DEFINE_PRIM(set_return_code,1);
DEFINE_PRIM(get_client_header,1);
DEFINE_PRIM(get_client_headers,0);
DEFINE_PRIM(parse_multipart_data,2);
DEFINE_PRIM(cgi_flush,0);
DEFINE_PRIM(cgi_get_config,0);
DEFINE_PRIM(cgi_set_config,1);
DEFINE_PRIM(cgi_command,1);
DEFINE_PRIM(get_http_method,0);

/* ************************************************************************ */
