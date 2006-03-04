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
#include "mod_neko.h"

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
	if( strcmp(val_string(s),"Content-Type") == 0 ) {
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
	int len = strlen(v);
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

static void parse_multipart( mcontext *c, const char *ctype, const char *args, int argslen, value *p ) {
	char *boundary = strstr(ctype,"boundary=");
	char *bend;
	char old, oldb1, oldb2;
	value vtmp;
	if( boundary == NULL )
		return;
	boundary += 9;
	PARSE_HEADER(boundary,bend);
	boundary-=2;
	oldb1 = *boundary;
	oldb2 = boundary[1];
	*boundary = '-';
	boundary[1] = '-';
	old = *bend;
	*bend = 0;
	{
		char *name, *end_name;
		char *data, *end_data;
		char tmp;
		name = strstr(args,boundary);
		while( name != NULL ) {
			name = strstr(name,"Content-Disposition:");
			if( name == NULL )
				break;
			name = strstr(name,"name=");
			if( name == NULL )
				break;
			name += 5;
			PARSE_HEADER(name,end_name);
			data = strstr(end_name,"\r\n\r\n");
			if( data == NULL )
				break;
			data += 4;
			end_data = memfind(data,argslen - (int)(data - args),boundary);
			if( end_data == NULL )
				break;
			tmp = *end_name;
			*end_name = 0;

			vtmp = alloc_array(3);
			val_array_ptr(vtmp)[0] = copy_string(name,(int)(end_name - name));
			val_array_ptr(vtmp)[1] = copy_string(data,(int)(end_data-data-2));
			val_array_ptr(vtmp)[2] = *p;
			*p = vtmp;

			*end_name = tmp;

			name = end_data;
		}
	}
	*boundary = oldb1;
	boundary[1] = oldb2;
	*bend = old;
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
	if( c->post_data != NULL ) {
		const char *ctype = ap_table_get(c->r->headers_in,"Content-Type");
		if( ctype && strstr(ctype,"multipart/form-data") != NULL )
			parse_multipart(c,ctype,val_string(c->post_data),val_strlen(c->post_data),&p);
		else if( ctype && strstr(ctype,"application/x-www-form-urlencoded") != NULL )
			parse_get(&p,val_string(c->post_data));
	}

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
	cgi_set_main : function:0 -> void
	<doc>Set the main entry point function</doc>
**/
static value cgi_set_main( value f ) {
	val_check_function(f,0);
	CONTEXT()->main = f;
	return val_true;
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

/* ************************************************************************ */
