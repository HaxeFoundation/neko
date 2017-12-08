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
#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>
#include "protocol.h"

#ifndef OS_WINDOWS
#	include <arpa/inet.h>
#	define strcmpi	strcasecmp
#endif

#define send_headers(c) \
	if( !c->headers_sent ) { \
		ap_send_http_header(c->r); \
		c->headers_sent = 1; \
	}

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
#	define REMOTE_ADDR(c)	c->remote_addr->sa.sin.sin_addr
#else
#	define LOG_SUCCESS
#	define REMOTE_ADDR(c)	c->remote_addr.sin_addr
#endif

#if AP_SERVER_MAJORVERSION_NUMBER >= 2 && AP_SERVER_MINORVERSION_NUMBER >= 4
#       define REMOTE_IP(r) r->useragent_ip
#else
#       define REMOTE_IP(r) r->connection->remote_ip
#endif

#define DEFAULT_HOST			"127.0.0.1"
#define DEFAULT_PORT			6666
#define DEFAULT_MAX_POST_DATA	(1 << 18) // 256 K

typedef struct {
	char *host;
	int port_min;
	int port_max;
	int max_post_size;
	int hits;
	bool proxy_mode;
} mconfig;

typedef struct {
	request_rec *r;
	proto *p;
	char *post_data;
	char *xff;
	char *client_ip;
	int post_data_size;
	bool headers_sent;
	bool is_multipart;
	bool is_form_post;
	bool need_discard;
} mcontext;

static mconfig config;
static bool init_done = false;

static int get_client_header( void *_c, const char *key, const char *val ) {
	mcontext *c = (mcontext*)_c;
	if( key == NULL || val == NULL )
		return 1;
	if( config.proxy_mode && strcmpi(key,"X-Forwarded-For") == 0 )
		protocol_send_header(c->p,key,c->xff);
	else
		protocol_send_header(c->p,key,val);
	return 1;
}

static void do_get_headers( void *_c ) {
	mcontext *c = (mcontext*)_c;
	ap_table_do(get_client_header,c,c->r->headers_in,NULL);
}

static void do_get_params( void *_c ) {
	mcontext *c = (mcontext*)_c;
	if( c->r->args )
		protocol_send_raw_params(c->p,c->r->args);
	if( c->post_data && c->is_form_post )
		protocol_send_raw_params(c->p,c->post_data);
}

static int do_print( void *_c, const char *buf, int len ) {
	mcontext *c = (mcontext*)_c;
	ap_soft_timeout("Client Timeout",c->r);
	send_headers(c);
	ap_rwrite(buf,len,c->r);
	ap_kill_timeout(c->r);
	return c->r->connection->aborted == 0;
}

static void do_flush( void *_c ) {
	mcontext *c = (mcontext*)_c;
	ap_rflush(c->r);
}

static void do_set_header( void *_c, const char *key, const char *value, bool add ) {
	mcontext *c = (mcontext*)_c;
	if( add )
		ap_table_add(c->r->headers_out,key,value);
	else if( strcmpi(key,"Content-Type") == 0 ) {
		int len = (int)strlen(value);
		char *ct = (char*)ap_palloc(c->r->pool,len+1);
		memcpy(ct,value,len+1);
		c->r->content_type = ct;
	} else
		ap_table_set(c->r->headers_out,key,value);
}

static void do_set_return_code( void *_c, int code ) {
	mcontext *c = (mcontext*)_c;
	c->r->status = code;
}

static void do_log( void *_c, const char *msg, bool user_log ) {
	mcontext *c = (mcontext*)_c;
	if( user_log ) {
		c->r->content_type = "text/plain";
		do_print(c,"Error : ",8);
		do_print(c,msg,(int)strlen(msg));
	} else
		ap_log_rerror(APLOG_MARK, APLOG_WARNING, LOG_SUCCESS c->r, "[mod_tora] %s", msg);
}

static void log_error( mcontext *c, const char *msg ) {
	do_log(c,msg,false); // add to apache log
	do_log(c,msg,true); // display to user
}

static int do_stream_data( void *_c, char *buf, int size ) {
	mcontext *c = (mcontext*)_c;
	// startup
	if( size == 0 ) {
		if( !ap_should_client_block(c->r) )
			return -1;
		c->need_discard = true;
		return 0;
	}
	return ap_get_client_block(c->r,buf,size);
}

static void discard_body( mcontext *c ) {
	char buf[1024];
	while( ap_get_client_block(c->r,buf,1024) > 0 ) {
	}
}

static int tora_handler( request_rec *r ) {
	mcontext ctx, *c = &ctx;
	if( strcmp(r->handler,"tora-handler") != 0)
		return DECLINED;

	// init context
	c->need_discard = false;
	c->is_multipart = false;
	c->headers_sent = false;
	c->is_form_post = false;
	c->r = r;
	c->post_data = NULL;
	c->xff = NULL;
	c->client_ip = NULL;
	c->p = NULL;
	c->r->content_type = "text/html";
	config.hits++;

	// read post data
	{
		const char *ctype = ap_table_get(r->headers_in,"Content-Type");
		ap_setup_client_block(r,REQUEST_CHUNKED_ERROR);
		if( ctype && strstr(ctype,"multipart/form-data") )
			c->is_multipart = true;
		else if( ap_should_client_block(r) ) {
			int tlen = 0;
			c->post_data = (char*)malloc(config.max_post_size);
			while( true ) {
				int len = ap_get_client_block(r,c->post_data + tlen,config.max_post_size - tlen);
				if( len <= 0 )
					break;
				tlen += len;
			}
			if( tlen >= config.max_post_size ) {
				discard_body(c);
				free(c->post_data);
				log_error(c,"Maximum POST data exceeded. Try using multipart encoding");
				return OK;
			}
			c->post_data[tlen] = 0;
			c->post_data_size = tlen;
			c->is_form_post = ctype == NULL || (strstr(ctype,"urlencoded") != NULL);
		}
	}

	// init protocol
	{
		protocol_infos infos;
		request_rec *first = r;
		while( first->prev != NULL )
			first = first->prev;
		infos.custom = c;
		infos.script = r->filename;
		infos.uri = first->uri;
		infos.hostname = r->hostname ? r->hostname : "";
		if( config.proxy_mode ) {
			const char *xff = ap_table_get(r->headers_in,"X-Forwarded-For");
			if( xff == NULL )
			    infos.client_ip = REMOTE_IP(r);
			else {
				char tmp;
				char *xend = (char*)xff + strlen(xff) - 1;
				while( xend > xff && *xend != ' ' && *xend != ',' )
					xend--;
				c->client_ip = strdup(xend);
				infos.client_ip = c->client_ip;
				if( xend > xff && *xend == ' ' && xend[-1] == ',' )
					xend--;
				tmp = *xend;
				*xend = 0;
				c->xff = strdup(xff);
				*xend = tmp;
			}
		} else
		    infos.client_ip = REMOTE_IP(r);
		infos.http_method = r->method;
		infos.get_data = r->args;
		infos.post_data = c->post_data;
		infos.post_data_size = c->post_data_size;
		infos.content_type = ap_table_get(r->headers_in,"Content-Type");
		infos.do_get_headers = do_get_headers;
		infos.do_get_params = do_get_params;
		infos.do_set_header = do_set_header;
		infos.do_set_return_code = do_set_return_code;
		infos.do_print = do_print;
		infos.do_flush = do_flush;
		infos.do_log = do_log;
		infos.do_stream_data = c->is_multipart ? do_stream_data : NULL;
		c->p = protocol_init(&infos);
	}

	// run protocol
	{
		int port = config.port_min + (config.hits % (1 + config.port_max - config.port_min));
		if( !protocol_connect(c->p,config.host,port) ||
			!protocol_send_request(c->p) ||
			!protocol_read_answer(c->p) )
			log_error(c,protocol_get_error(c->p));
	}

	// cleanup
	protocol_free(c->p);
	free(c->xff);
	free(c->client_ip);
	free(c->post_data);
	send_headers(c); // in case...
	if( c->need_discard )
		discard_body(c);
	return OK;
}

static void mod_tora_do_init() {
	int tmp = 0;
	if( init_done ) return;
	init_done = true;
	memset(&config,0,sizeof(config));
	config.host = DEFAULT_HOST;
	config.port_min = DEFAULT_PORT;
	config.port_max = DEFAULT_PORT;
	config.max_post_size = DEFAULT_MAX_POST_DATA;
}

#ifdef APACHE_2_X
#	define MCONFIG void*
#else
#	define MCONFIG char*
#endif
static const char *mod_tora_config( cmd_parms *cmd, MCONFIG mconfig, const char *fargs ) {
	char *code = strdup(fargs);
	char *args = code;
	int value;
	while( true ) {
		char c = *args;
		if( c == 0 || c == ' ' || c == '\t' ) break;
		args++;
	}
	while( *args == ' ' || *args == '\t' )
		*args++ = 0;
	value = atoi(args);
	mod_tora_do_init();
	if( strcmp(code,"HOST") == 0 ) config.host = strdup(args);
	else if( strcmp(code,"PORT") == 0 ) { config.port_min = value; config.port_max = value; }
	else if( strcmp(code,"PORT_MAX") == 0 ) config.port_max = value;
	else if( strcmp(code,"POST_SIZE") == 0 ) config.max_post_size = value;
	else if( strcmp(code,"PROXY_MODE") == 0 ) config.proxy_mode = value;
	else ap_log_error(APLOG_MARK,APLOG_WARNING,LOG_SUCCESS cmd->server,"Unknown ModTora configuration command '%s'",code);
	free(code);
	return NULL;
}

#ifdef APACHE_2_X
static int tora_init( apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s ) {
	mod_tora_do_init();
	return OK;
}
#else
static void tora_init(server_rec *s, pool *p) {
	mod_tora_do_init();
}
#endif

static command_rec tora_module_cmds[] = {
#	ifdef APACHE_2_X
	AP_INIT_RAW_ARGS( "ModTora", mod_tora_config , NULL, RSRC_CONF, NULL ),
#	else
	{ "ModTora", mod_tora_config, NULL, RSRC_CONF, RAW_ARGS, NULL },
#	endif
	{ NULL }
};

#ifdef APACHE_2_X

static void tora_register_hooks( apr_pool_t *p ) {
	ap_hook_post_config( tora_init, NULL, NULL, APR_HOOK_MIDDLE );
	ap_hook_handler( tora_handler, NULL, NULL, APR_HOOK_LAST );
};

module AP_MODULE_DECLARE_DATA tora_module = {
	STANDARD20_MODULE_STUFF,
	NULL,
	NULL,
	NULL,
	NULL,
	tora_module_cmds,
	tora_register_hooks
};

#else /* APACHE 1.3 */

static const handler_rec tora_handlers[] = {
    {"tora-handler", tora_handler},
    {NULL}
};

module MODULE_VAR_EXPORT tora_module = {
    STANDARD_MODULE_STUFF,
    tora_init,
    NULL,
    NULL,
    NULL,
    NULL,
    tora_module_cmds,
    tora_handlers,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

#endif

/* ************************************************************************ */
