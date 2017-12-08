/*
 * Copyright (C)2005-2017 Haxe Foundation
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
#include <string.h>
#include <stdlib.h>
#include "protocol.h"
#include "socket.h"

struct _protocol {
	PSOCK s;
	bool error;
	char *error_msg;
	protocol_infos inf;
};

typedef enum {
	CODE_FILE = 1,
	CODE_URI,
	CODE_CLIENT_IP,
	CODE_GET_PARAMS,
	CODE_POST_DATA,
	CODE_HEADER_KEY,
	CODE_HEADER_VALUE,
	CODE_HEADER_ADD_VALUE,
	CODE_PARAM_KEY,
	CODE_PARAM_VALUE,
	CODE_HOST_NAME,
	CODE_HTTP_METHOD,
	CODE_EXECUTE,
	CODE_ERROR,
	CODE_PRINT,
	CODE_LOG,
	CODE_FLUSH,
	CODE_REDIRECT,
	CODE_RETURNCODE,
	CODE_QUERY_MULTIPART,
	CODE_PART_FILENAME,
	CODE_PART_KEY,
	CODE_PART_DATA,
	CODE_PART_DONE,
	CODE_TEST_CONNECT,
	CODE_LISTEN,
} proto_code;

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

static bool proto_error( proto *p, const char *error ) {
	free(p->error_msg);
	p->error_msg = strdup(error);
	p->error = true;
	return false;
}

proto *protocol_init( protocol_infos *inf ) {
	proto *p = (proto*)malloc(sizeof(struct _protocol));
	p->s = INVALID_SOCKET;
	p->inf = *inf;
	p->error = false;
	p->error_msg = NULL;
	psock_init();
	return p;
}

bool protocol_connect( proto *p, const char *host, int port ) {
	PHOST h = phost_resolve(host);
	if( h == UNRESOLVED_HOST )
		return proto_error(p,"Failed to resolve host");
	p->s = psock_create();
	if( p->s == INVALID_SOCKET )
		return proto_error(p,"Failed to create socket");
	if( psock_connect(p->s,h,port) != PS_OK )
		return proto_error(p,"Failed to connect to TORA host");
	return true;
}

const char *protocol_get_error( proto *p ) {
	return p->error_msg ? p->error_msg : "NO ERROR";
}

void protocol_free( proto *p ) {
	psock_close(p->s);
	free(p->error_msg);
	free(p);
}

static void proto_write( proto *p, const char *str, int len ) {
	while( len ) {
		int b = psock_send(p->s,str,len);
		if( b <= 0 ) {
			p->error = true;
			return;
		}
		len -= b;
		str += b;
	}
}

static bool proto_read( proto *p, char *str, int len ) {
	while( len ) {
		int b = psock_recv(p->s,str,len);
		if( b <= 0 ) {
			p->error = true;
			return false;
		}
		len -= b;
		str += b;
	}
	return true;
}

static void proto_send_size( proto *p, proto_code code, const char *str, int len ) {
	unsigned char h[4];
	h[0] = (unsigned char)code;
	h[1] = (unsigned char)len;
	h[2] = (unsigned char)(len >> 8);
	h[3] = (unsigned char)(len >> 16);
	proto_write(p,(char*)h,4);
	proto_write(p,str,len);
}

static void proto_send( proto *p, proto_code code, const char *str ) {
	proto_send_size(p,code,str,(int)strlen(str));
}

void protocol_send_header( proto *p, const char *key, const char *val ) {
	proto_send(p,CODE_HEADER_KEY,key);
	proto_send(p,CODE_HEADER_VALUE,val);
}

static int url_decode( const char *bin, int len, char *bout ) {
	int pin = 0;
	int pout = 0;
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

#define DEFAULT_SIZE	256

static void proto_send_decode( proto *p, proto_code code, const char *str, int len ) {
	char tmp[DEFAULT_SIZE];
	char *buf = NULL;
	int size;
	if( len >= DEFAULT_SIZE )
		buf = malloc(len+1);
	size = url_decode(str,len,buf?buf:tmp);
	proto_send_size(p,code,buf?buf:tmp,size);
	if( buf )
		free(buf);
}

void protocol_send_param( proto *p, const char *param, int param_size, const char *value, int value_size ) {
	proto_send_size(p,CODE_PARAM_KEY,param,param_size);
	proto_send_size(p,CODE_PARAM_VALUE,value,value_size);
}

void protocol_send_raw_params( proto *p, const char *args ) {
	char *aand, *aeq, *asep;
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
			proto_send_decode(p,CODE_PARAM_KEY,args,(int)(aeq-args));
			proto_send_decode(p,CODE_PARAM_VALUE,aeq+1,(int)strlen(aeq+1));
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

bool protocol_send_request( proto *p ) {
	proto_send(p,CODE_FILE,p->inf.script);
	proto_send(p,CODE_URI,p->inf.uri);
	proto_send(p,CODE_HOST_NAME,p->inf.hostname);
	proto_send(p,CODE_CLIENT_IP,p->inf.client_ip);
	if( p->inf.do_get_headers )
		p->inf.do_get_headers(p->inf.custom);
	if( p->inf.get_data )
		proto_send(p,CODE_GET_PARAMS,p->inf.get_data);
	if( p->inf.post_data )
		proto_send_size(p,CODE_POST_DATA,p->inf.post_data,p->inf.post_data_size);
	if( p->inf.do_get_params )
		p->inf.do_get_params(p->inf.custom);
	proto_send(p,CODE_HTTP_METHOD,p->inf.http_method);
	psock_set_fastsend(p->s,1);
	proto_send_size(p,CODE_EXECUTE,NULL,0);
	psock_set_fastsend(p->s,0);
	return !p->error;
}

static int fill_buffer( proto *p, char *buf, int bufsize, int pos ) {
	while( pos < bufsize ) {
		int k = p->inf.do_stream_data(p->inf.custom,buf+pos,bufsize-pos);
		if( k <= 0 ) break;
		pos += k;
	}
	return pos;
}

static bool send_multipart_data( proto *p, char *buf, int bufsize ) {
	int len = 0;
	char *boundstr = NULL;
	int boundstr_len;
	if( p->inf.content_type == NULL || p->inf.do_stream_data == NULL )
		return true;
	// extract boundary value
	{
		const char *boundary, *bend;
		if( (boundary = strstr(p->inf.content_type,"boundary=")) == NULL )
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
	// permit the server to start download if needed
	if( p->inf.do_stream_data(p->inf.custom,NULL,0) != 0 ) {
		free(boundstr);
		return false;
	}
	while( true ) {
		char *name, *end_name, *filename, *end_file_name, *data;
		int pos;
		// refill buffer
		// we assume here that the the whole multipart header can fit in the buffer
		len = fill_buffer(p,buf,bufsize,len);
		// is boundary at the beginning of buffer ?
		if( len < boundstr_len || memcmp(buf,boundstr,boundstr_len) != 0 ) {
			free(boundstr);
			return false;
		}
		name = memfind(buf,len,"Content-Disposition:");
		if( name == NULL )
			break;
		name = memfind(name,len - (int)(name - buf),"name=");
		if( name == NULL ) {
			free(boundstr);
			return false;
		}
		name += 5;
		PARSE_HEADER(name,end_name);
		data = memfind(end_name,len - (int)(end_name - buf),"\r\n\r\n");
		if( data == NULL ) {
			free(boundstr);
			return false;
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
			proto_send_size(p,CODE_PART_FILENAME,filename,(int)(end_file_name - filename));
		proto_send_size(p,CODE_PART_KEY,name,(int)(end_name - name));
		// read data
		while( true ) {
			const char *boundary;
			// recall buffer
			memmove(buf,buf+pos,len - pos);
			len -= pos;
			pos = 0;
			len = fill_buffer(p,buf,bufsize,len);
			// lookup bounds
			boundary = memfind(buf,len,boundstr);
			if( boundary == NULL ) {
				if( len == 0 ) {
					free(boundstr);
					return false;
				}
				// send as much buffer as possible to client
				if( len < bufsize )
					pos = len;
				else
					pos = len - boundstr_len + 1;
				proto_send_size(p,CODE_PART_DATA,buf,pos);
			} else {
				// send remaining data
				pos = (int)(boundary - buf);
				proto_send_size(p,CODE_PART_DATA,buf,pos - 2);
				// recall
				memmove(buf,buf+pos,len - pos);
				len -= pos;
				break;
			}
		}
		proto_send_size(p,CODE_PART_DONE,"",0);
	}
	free(boundstr);
	return true;
}

#define BUFSIZE	(1 << 16) // 64 KB
#define ABORT(msg)	{ proto_error(p,msg); goto exit; }

bool protocol_read_answer( proto *p ) {
	unsigned char header[4];
	int len;
	char *buf = (char*)malloc(BUFSIZE), *key = NULL;
	int buflen = BUFSIZE;
	int listening = 0;
	while( true ) {
		if( !proto_read(p,header,4) )
			ABORT("Connection Closed");
		len = header[1] | (header[2] << 8) | (header[3] << 16);
		if( buflen <= len ) {
			while( buflen <= len )
				buflen <<= 1;
			free(buf);
			buf = (char*)malloc(buflen);
		}
		if( !proto_read(p,buf,len) )
			ABORT("Connection Closed");
		buf[len] = 0;
		switch( *header ) {
		case CODE_HEADER_KEY:
			key = strdup(buf);
			break;
		case CODE_HEADER_VALUE:
			if( !key ) ABORT("Missing key");
			p->inf.do_set_header(p->inf.custom,key,buf,false);
			free(key); key = NULL;
			break;
		case CODE_HEADER_ADD_VALUE:
			if( !key ) ABORT("Missing key");
			p->inf.do_set_header(p->inf.custom,key,buf,true);
			free(key); key = NULL;
			break;
		case CODE_EXECUTE:
			goto exit;
		case CODE_ERROR:
			p->inf.do_log(p->inf.custom,buf,true);
			goto exit;
		case CODE_PRINT:
			if( !p->inf.do_print(p->inf.custom,buf,len) && listening )
				goto exit;
			if( listening )
				p->inf.do_flush(p->inf.custom);
			break;
		case CODE_LOG:
			p->inf.do_log(p->inf.custom,buf,false);
			break;
		case CODE_FLUSH:
			p->inf.do_flush(p->inf.custom);
			break;
		case CODE_REDIRECT:
			p->inf.do_set_header(p->inf.custom,"Location",buf,false);
			p->inf.do_set_return_code(p->inf.custom,302);
			break;
		case CODE_RETURNCODE:
			p->inf.do_set_return_code(p->inf.custom,atoi(buf));
			break;
		case CODE_QUERY_MULTIPART:
			{
				int tmpsize = atoi(buf);
				char *tmp = (char*)malloc(tmpsize + 1);
				tmp[tmpsize] = 0;
				if( !send_multipart_data(p,tmp,tmpsize) ) {
					free(tmp);
					ABORT("Failed to send multipart data");
				}
				free(tmp);
				proto_send(p,CODE_EXECUTE,"");
			}
			break;
		case CODE_LISTEN:
			listening = 1;
			p->inf.do_flush(p->inf.custom);
			break;
		default:
			ABORT("Unexpected code");
		}
	}
exit:
	free(key);
	free(buf);
	return !p->error;
}

/* ************************************************************************ */
