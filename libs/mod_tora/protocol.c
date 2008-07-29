/* ************************************************************************ */
/*																			*/
/*  Tora - Neko Application Server											*/
/*  Copyright (c)2008 Motion-Twin											*/
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
#include "protocol.h"
#include <string.h>

#ifndef NEKO_WINDOWS
#	define strcmpi	strcasecmp
#endif

#if defined(NEKO_WINDOWS) || defined(NEKO_MAC)
#	define MSG_NOSIGNAL 0
#endif

#define MAX_PARAM_SIZE		1024

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

static int pwrite( mcontext *c, const char *buf, int len ) {
	while( len > 0 ) {
		int k = send(c->sock,buf,len,MSG_NOSIGNAL);
		if( k <= 0 ) return 0;
		buf += k;
		len -= k;
	}
	return 1;
}

static int pread( mcontext *c, char *buf, int len ) {
	while( len > 0 ) {
		int k = recv(c->sock,buf,len,MSG_NOSIGNAL);
		if( k <= 0 ) return 0;
		buf += k;
		len -= k;
	}
	return 1;
}

static void psend_size( mcontext *c, proto_code code, const char *str, int len ) {
	unsigned char h[4];
	h[0] = (unsigned char)code;
	h[1] = (unsigned char)len;
	h[2] = (unsigned char)(len >> 8);
	h[3] = (unsigned char)(len >> 16);
	pwrite(c,(char*)h,4);
	pwrite(c,str,len);
}

static void psend( mcontext *c, proto_code code, const char *str ) {
	psend_size(c,code,str,strlen(str));
}

static int send_client_header( void *_c, const char *key, const char *val ) {
	mcontext *c = (mcontext*)_c;
	if( key == NULL || val == NULL )
		return 1;
	psend(c,CODE_HEADER_KEY,key);
	psend(c,CODE_HEADER_VALUE,val);
	return 1;
}

static int url_decode( const char *bin, int len, char *bout ) {
	int pin = 0;
	int pout = 0;
	if( len >= MAX_PARAM_SIZE ) {
		*bout = 0;
		return 0;
	}
	while( len-- > 0 ) {
		char c = bin[pin++];
		if( c == '+' )
			c = ' ';
		else if( c == '%' ) {
			int p1, p2;
			if( len < 2 )
				break;
			p1 = bin[pin++];
			p2 = bin[pin++];
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
		bout[pout++] = c;
	}
	bout[pout] = 0;
	return pout;
}

static void send_parsed_params( mcontext *ctx, const char *args ) {
	char *aand, *aeq, *asep;
	char tmp[MAX_PARAM_SIZE];
	int size;
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
			size = url_decode(args,(int)(aeq-args),tmp);
			psend_size(ctx,CODE_PARAM_KEY,tmp,size);
			size = url_decode(aeq+1,(int)strlen(aeq+1),tmp);
			psend_size(ctx,CODE_PARAM_VALUE,tmp,size);
			*aeq = '=';
		}
		if( aand == NULL )
			break;
		*aand = (aand == asep)?';':'&';
		args = aand+1;
	}
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

static void fill_buffer( mcontext *c, char *buf, int bufsize, int *len ) {
	int pos = *len;
	while( pos < bufsize ) {
		int k = ap_get_client_block(c->r,buf+pos,bufsize-pos);
		if( k == 0 )
			break;
		pos += k;
	}
	*len = pos;
}

static int discard_body( mcontext *c, char *buf, int bufsize ) {
	while( ap_get_client_block(c->r,buf,bufsize) > 0 ) {
	}
	return false;
}

static bool send_multipart_data( mcontext *c, char *buf, int bufsize ) {
	int len = 0;
	const char *ctype = ap_table_get(c->r->headers_in,"Content-Type");
	char *boundstr = NULL;
	int boundstr_len;
	if( !ctype || strstr(ctype,"multipart/form-data") == NULL )
		return true;
	// extract boundary value
	{
		const char *boundary, *bend;
		if( (boundary = strstr(ctype,"boundary=")) == NULL )
			return false;
		boundary += 9;
		PARSE_HEADER(boundary,bend);
		len = (int)(bend - boundary);
		boundstr_len = len + 2;
		if( boundstr_len > bufsize / 2 )
			return false;
		boundstr = (char*)malloc(boundstr_len + 1);
		boundstr[0] = '-';
		boundstr[1] = '-';
		boundstr[boundstr_len] = 0;
		memcpy(boundstr+2,boundary,len);
	}
	len = 0;
	if( !ap_should_client_block(c->r) ) {
		free(boundstr);
		return false;
	}
	while( true ) {
		char *name, *end_name, *filename, *end_file_name, *data;
		int pos;
		// refill buffer
		// we assume here that the the whole multipart header can fit in the buffer
		fill_buffer(c,buf,bufsize,&len);
		// is boundary at the beginning of buffer ?
		if( len < boundstr_len || memcmp(buf,boundstr,boundstr_len) != 0 ) {
			free(boundstr);
			return discard_body(c,buf,bufsize);
		}
		name = memfind(buf,len,"Content-Disposition:");
		if( name == NULL )
			break;
		name = memfind(name,len - (int)(name - buf),"name=");
		if( name == NULL ) {
			free(boundstr);
			return discard_body(c,buf,bufsize);
		}
		name += 5;
		PARSE_HEADER(name,end_name);
		data = memfind(end_name,len - (int)(end_name - buf),"\r\n\r\n");
		if( data == NULL ) {
			free(boundstr);
			return discard_body(c,buf,bufsize);
		}
		filename = memfind(name,(int)(data - name),"filename=");
		if( filename != NULL ) {
			filename += 9;
			PARSE_HEADER(filename,end_file_name);
		}
		data += 4;
		pos = (int)(data - buf);
		// send part name
		if( filename )
			psend_size(c,CODE_PART_FILENAME,filename,(int)(end_file_name - filename));
		psend_size(c,CODE_PART_KEY,name,(int)(end_name - name));
		// read data
		while( true ) {
			const char *boundary;
			// recall buffer
			memcpy(buf,buf+pos,len - pos);
			len -= pos;
			pos = 0;
			fill_buffer(c,buf,bufsize,&len);
			// lookup bounds
			boundary = memfind(buf,len,boundstr);
			if( boundary == NULL ) {
				if( len == 0 ) {
					free(boundstr);
					return discard_body(c,buf,len);
				}
				// send as much buffer as possible to client
				if( len < bufsize )
					pos = len;
				else
					pos = len - boundstr_len + 1;
				psend_size(c,CODE_PART_DATA,buf,pos);
			} else {
				// send remaining data
				pos = (int)(boundary - buf);
				psend_size(c,CODE_PART_DATA,buf,pos - 2);
				// recall
				memcpy(buf,buf+pos,len - pos);
				len -= pos;
				break;
			}
		}
		psend_size(c,CODE_PART_DONE,"",0);
	}
	free(boundstr);
	return true;
}

void protocol_send_request( mcontext *c ) {
	request_rec *tmp = c->r;
	while( tmp->prev != NULL )
		tmp = tmp->prev;
	psend(c,CODE_FILE,c->r->filename);
	psend(c,CODE_URI,tmp->uri);
	psend(c,CODE_HOST_NAME, c->r->connection->local_host ? c->r->connection->local_host : c->r->connection->local_ip );
	psend(c,CODE_CLIENT_IP, c->r->connection->remote_ip );
	ap_table_do(send_client_header,c,c->r->headers_in,NULL);
	if( c->r->args != NULL ) {
		psend(c,CODE_GET_PARAMS,c->r->args);
		send_parsed_params(c,c->r->args);
	}
	if( c->post_data != NULL ) {
		psend_size(c,CODE_POST_DATA,c->post_data,c->post_data_size);
		send_parsed_params(c,c->post_data);
	}
	psend(c,CODE_HTTP_METHOD,c->r->method);
	psend_size(c,CODE_EXECUTE,NULL,0);
}


#define BUFSIZE	(1 << 16) // 64 KB
#define ABORT(msg)	{ error = strdup(msg); goto exit; }

char *protocol_loop( mcontext *c ) {
	unsigned char header[4];
	int len;
	char *buf = (char*)malloc(BUFSIZE), *key = NULL;
	char *error = NULL;
	int buflen = BUFSIZE;
	while( true ) {
		if( !pread(c,header,4) )
			ABORT("Connection Closed");
		len = header[1] | (header[2] << 8) | (header[3] << 16);
		if( buflen <= len ) {
			while( buflen < len )
				buflen <<= 1;
			free(buf);
			buf = (char*)malloc(buflen);
		}
		if( !pread(c,buf,len) )
			ABORT("Connection Closed");
		buf[len] = 0;
		switch( *header ) {
		case CODE_HEADER_KEY:
			if( c->headers_sent )
				ABORT("Cannot set header : headers already sent");
			key = strdup(buf);
			break;
		case CODE_HEADER_VALUE:
			if( strcmpi(key,"Content-Type") == 0 ) {
				char *ct = (char*)ap_palloc(c->r->pool,len+1);
				memcpy(ct,buf,len+1);
				c->r->content_type = ct;
			} else
				ap_table_set(c->r->headers_out,key,buf);
			free(key); key = NULL;
			break;
		case CODE_HEADER_ADD_VALUE:
			ap_table_add(c->r->headers_out,key,buf);
			free(key); key = NULL;
			break;
		case CODE_EXECUTE:
			goto exit;
		case CODE_ERROR:
			ABORT(buf);
		case CODE_PRINT:
			ap_soft_timeout("Client Timeout",c->r);
			send_headers(c);
			ap_rwrite(buf,len,c->r);
			ap_kill_timeout(c->r);
			break;
		case CODE_LOG:
			ap_log_rerror(__FILE__, __LINE__, APLOG_NOTICE, LOG_SUCCESS c->r, "[mod_tora] %s", buf);
			break;
		case CODE_FLUSH:
			ap_rflush(c->r);
			break;
		case CODE_REDIRECT:
			if( c->headers_sent )
				ABORT("Cannot redirect : headers already sent");
			ap_log_rerror(__FILE__, __LINE__, APLOG_NOTICE, LOG_SUCCESS c->r, "[mod_tora] REDIR[%s]", buf);
			ap_table_set(c->r->headers_out,"Location",buf);
			c->r->status = REDIRECT;
			break;
		case CODE_RETURNCODE:
			if( c->headers_sent )
				ABORT("Cannot set return code : headers already sent");
			c->r->status = atoi(buf);
			break;
		case CODE_QUERY_MULTIPART:
			{
				int tmpsize = atoi(buf);
				char *tmp = (char*)malloc(tmpsize + 1);
				tmp[tmpsize] = 0;
				if( !send_multipart_data(c,tmp,tmpsize) ) {
					free(tmp);
					ABORT("Failed to parse multipart data");
				}
				free(tmp);
				psend(c,CODE_EXECUTE,"");
			}
			break;
		default:
			ABORT("Unexpected code");
		}
	}
exit:
	free(key);
	free(buf);
	return error;
}

/* ************************************************************************ */
