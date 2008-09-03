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
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>

#ifdef STANDARD20_MODULE_STUFF
#	define APACHE_2_X
#	define ap_send_http_header(x)
#	define ap_soft_timeout(msg,r)
#	define ap_kill_timeout(r)
#	define ap_table_get		apr_table_get
#	define ap_table_set		apr_table_set
#	define ap_table_add		apr_table_add
#	define ap_table_do		apr_table_do
#	define ap_palloc		apr_palloc
#	define LOG_SUCCESS		APR_SUCCESS,
#	define REDIRECT			HTTP_MOVED_TEMPORARILY
#else
#	define LOG_SUCCESS
#endif

#undef INLINE
#undef closesocket

#include <neko.h>
// neko is only used for some #ifdef, no primitive is linked here
#ifdef NEKO_WINDOWS
#	include <winsock2.h>
#else
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <unistd.h>
#	include <netdb.h>
	typedef int SOCKET;
#	define closesocket close
#	define SOCKET_ERROR (-1)
#	define INVALID_SOCKET (-1)
#endif

typedef struct {
	request_rec *r;
	SOCKET sock;
	char *post_data;
	int post_data_size;
	int headers_sent;
} mcontext;

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
} proto_code;

#define send_headers(c) \
	if( !c->headers_sent ) { \
		ap_send_http_header(c->r); \
		c->headers_sent = true; \
	}

void protocol_send_request( mcontext *c );
char *protocol_loop( mcontext *c, int *exc );

#endif
