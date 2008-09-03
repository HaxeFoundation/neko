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

#define DEFAULT_HOST			"127.0.0.1"
#define DEFAULT_PORT			6666
#define DEFAULT_MAX_POST_DATA	(1 << 18) // 256 K

typedef struct {
	char *host;
	int port;
	int max_post_size;
	int hits;
} mconfig;

static mconfig config;
static int init_done = 0;

static void request_error( mcontext *c, const char *error, bool log ) {
	c->r->content_type = "text/plain";
	ap_soft_timeout("Client Timeout",c->r);
	send_headers(c);
	if( log )
		ap_rprintf(c->r,"Error : %s",error);
	else
		ap_rprintf(c->r,"Error : %s\n\n",error);
	ap_kill_timeout(c->r);
	if( log )
		ap_log_rerror(__FILE__, __LINE__, APLOG_WARNING, LOG_SUCCESS c->r, "[mod_tora error] %s", error);
}

static int error( mcontext *c, const char *msg ) {
	request_error(c,msg,true);
	free(c->post_data);
	closesocket(c->sock);
	return OK;
}

static int tora_handler( request_rec *r ) {
	mcontext _ctx, *ctx = &_ctx;

	// init context
	ctx->headers_sent = 0;
	ctx->r = r;
	ctx->post_data = NULL;
	ctx->sock = INVALID_SOCKET;
	ctx->r->content_type = "text/html";
	if( strcmp(r->handler,"tora-handler") != 0)
		return DECLINED;
	config.hits++;

	// read post data
	{
		const char *ctype = ap_table_get(r->headers_in,"Content-Type");
		ap_setup_client_block(r,REQUEST_CHUNKED_ERROR);
		if( (!ctype || strstr(ctype,"multipart/form-data") == NULL) && ap_should_client_block(r) ) {
			int tlen = 0;
			ctx->post_data = (char*)malloc(config.max_post_size);
			while( true ) {
				int len = ap_get_client_block(r,ctx->post_data + tlen,config.max_post_size - tlen);
				if( len <= 0 )
					break;
				tlen += len;
			}
			if( tlen >= config.max_post_size )
				return error(ctx,"Maximum POST data exceeded. Try using multipart encoding");
			ctx->post_data[tlen] = 0;
			ctx->post_data_size = tlen;
		}
	}

	// init socket
	ctx->sock = socket(AF_INET,SOCK_STREAM,0);
	if( ctx->sock == INVALID_SOCKET )
		return error(ctx,"Failed to create socket");
#	ifdef NEKO_MAC
	setsockopt(ctx->sock,SOL_SOCKET,SO_NOSIGPIPE,NULL,0);
#	endif

	// connect to host
	{
		unsigned int ip = inet_addr(config.host);
		struct sockaddr_in addr;
		if( ip == INADDR_NONE ) {
			struct hostent *h = (struct hostent*)gethostbyname(config.host);
			if( h == NULL )
				return error(ctx,"Failed to resolve TORA host");
			ip = *((unsigned int*)h->h_addr);
		}
		memset(&addr,0,sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(config.port);
		*(int*)&addr.sin_addr.s_addr = ip;
		if( connect(ctx->sock,(struct sockaddr*)&addr,sizeof(addr)) != 0 )
			return error(ctx,"Failed to connect to TORA host");
	}

	// send request infos
	protocol_send_request(ctx);

	// wait and handle response
	{
		int exc = 0;
		char *emsg = protocol_loop(ctx,&exc);
		if( emsg != NULL ) {
			request_error(ctx,emsg,!exc);
			free(emsg);
		}
	}

	// cleanup
	closesocket(ctx->sock);
	free(ctx->post_data);
	send_headers(ctx);
	return OK;
}

static void mod_tora_do_init() {
	int tmp = 0;
	if( init_done ) return;
	init_done = 1;
	memset(&config,0,sizeof(config));
	config.host = DEFAULT_HOST;
	config.port = DEFAULT_PORT;
	config.max_post_size = DEFAULT_MAX_POST_DATA;
#	ifdef _WIN32
	{
		WSADATA init_data;
		WSAStartup(MAKEWORD(2,0),&init_data);
	}
#	endif
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
	else if( strcmp(code,"PORT") == 0 ) config.port = value;
	else if( strcmp(code,"POST_SIZE") == 0 ) config.max_post_size = value;
	else ap_log_error(__FILE__,__LINE__,APLOG_WARNING,LOG_SUCCESS cmd->server,"Unknown ModTora configuration command '%s'",code);
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
