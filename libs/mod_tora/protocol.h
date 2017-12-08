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
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "osdef.h"

typedef void (*pf_callback)( void *custom );
typedef int (*pf_print)( void *custom, const char *buffer, int size );
typedef void (*pf_set_header)( void *custom, const char *key, const char *value, bool add );
typedef void (*pf_set_code)( void *custom, int code );
typedef void (*pf_log)( void *custom, const char *message, bool inner_log );
typedef int (*pf_stream_data)( void *custom, char *buffer, int size );

typedef struct {
	const char *script;
	const char *uri;
	const char *hostname;
	const char *client_ip;
	const char *http_method;
	const char *get_data;
	const char *post_data;
	const char *content_type;
	int post_data_size;
	pf_callback do_get_headers;
	pf_callback do_get_params;
	pf_print do_print;
	pf_callback do_flush;
	pf_set_header do_set_header;
	pf_set_code do_set_return_code;
	pf_stream_data do_stream_data;
	pf_log do_log;
	void *custom;
} protocol_infos;

struct _protocol;
typedef struct _protocol proto;

proto *protocol_init( protocol_infos *inf );
bool protocol_connect( proto *p, const char *host, int port );
bool protocol_send_request( proto *p );
void protocol_send_header( proto *p, const char *header, const char *value );
void protocol_send_param( proto *p, const char *param, int param_size, const char *value, int value_size );
void protocol_send_raw_params( proto *p, const char *data );
bool protocol_read_answer( proto *p );
const char *protocol_get_error( proto *p );
void protocol_free( proto *p );

#endif

/* ************************************************************************ */
