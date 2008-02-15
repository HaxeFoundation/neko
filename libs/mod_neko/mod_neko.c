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
#include "mod_neko.h"

#ifndef MOD_NEKO_POST_SIZE
#	define MOD_NEKO_POST_SIZE (1 << 18) // 256 K
#endif

#ifdef APACHE_2_X
#	define FTIME(r)		r->finfo.mtime
#	define ap_send_http_header(x)
#	define ap_soft_timeout(msg,r)
#	define ap_kill_timeout(r)
#	define ap_table_get		apr_table_get
#	define LOG_SUCCESS		APR_SUCCESS,
typedef apr_time_t aptime;
#else
#	define FTIME(r)		r->finfo.st_mtime
#	define LOG_SUCCESS
typedef time_t aptime;
#endif

#define apache_error(level,request,message)	\
	ap_rprintf(request,"<b>Error</b> : %s",message); \
	ap_log_rerror(__FILE__, __LINE__, level, LOG_SUCCESS request, "[mod_neko error] %s", message)

typedef struct cache {
	value file;
	value main;
	int hits;
	aptime time;
	struct cache *next;
} cache;

static mconfig config;
static int init_done = 0;
static _context *cache_root = NULL;

extern void neko_stats_measure( neko_vm *vm, const char *kind, int start );
extern value neko_stats_build( neko_vm *vm );

value cgi_command( value v ) {
	val_check(v,string);
	if( strcmp(val_string(v),"stats") == 0 )
		return neko_stats_build(neko_vm_current());
	if( strcmp(val_string(v),"cache") == 0 ) {
		cache *c = (cache*)context_get(cache_root);
		value l = val_null;
		while( c != NULL ) {
			value a = alloc_array(4);
			val_array_ptr(a)[0] = c->file;
			val_array_ptr(a)[1] = c->main;
			val_array_ptr(a)[2] = alloc_int(c->hits);
			val_array_ptr(a)[3] = l;
			l = a;
			c = c->next;
		}
		return l;
	}
	neko_error();
}

mconfig *mod_neko_get_config() {
	return &config;
}

void mod_neko_set_config( mconfig *c ) {
	config = *c;
}

static void gc_major() {
	if( config.gc_period <= 0 || config.hits % config.gc_period != 0 ) return;
	if( config.use_stats ) neko_stats_measure(NULL,"gc",1);
	neko_gc_major();
	if( config.use_stats ) neko_stats_measure(NULL,"gc",0);
}

static void send_headers( mcontext *c ) {
	if( !c->headers_sent ) {
		ap_send_http_header(c->r);
		c->headers_sent = true;
	}
}

static void request_print( const char *data, int size, void *_c ) {
	mcontext *c = (mcontext *)_c;
	if( c == NULL ) c = CONTEXT();
	if( size == -1 ) size = (int)strlen(data);
	ap_soft_timeout("Client Timeout",c->r);
	send_headers(c);
	ap_rwrite(data,size,c->r);
	ap_kill_timeout(c->r);
}

static value cache_find( request_rec *r ) {
	cache *c = (cache*)context_get(cache_root);
	cache *prev = NULL;
	value fname = alloc_string(r->filename);
	while( c != NULL ) {
		if( val_compare(fname,c->file) == 0 ) {
			if( config.use_cache && FTIME(r) == c->time ) {
				c->hits++;
				return c->main;
			}
			if( prev == NULL )
				context_set(cache_root,c->next);
			else
				prev->next = c->next;
			free_root((value*)c);
			// try to lower memory partitioning
			// when a module is updated
			c = NULL;
			gc_major();
			break;
		}
		prev = c;
		c = c->next;
	}
	return NULL;
}

static char *request_base_uri( request_rec *r ) {
	while( r->prev != NULL )
		r = r->prev;
	return r->unparsed_uri;
}

static void cache_module( request_rec *r, value main ) {
	cache *c = (cache*)context_get(cache_root);
	value fname = alloc_string(r->filename);
	while( c != NULL ) {
		if( val_compare(fname,c->file) == 0 ) {
			c->main = main;
			c->time = FTIME(r);
			return;
		}
		c = c->next;
	}
	c = (cache*)alloc_root(sizeof(struct cache) / sizeof(value));
	c->file = fname;
	c->main = main;
	c->time = FTIME(r);
	c->hits = 0;
	c->next = (cache*)context_get(cache_root);
	context_set(cache_root,c);
}

static int neko_handler_rec( request_rec *r ) {
	mcontext ctx;
	neko_vm *vm;
	const char *ctype;
	value exc = NULL;

	config.hits++;

	neko_set_stack_base(&ctx);
	ctx.r = r;
	ctx.main = cache_find(r);
	ctx.post_data = val_null;
	ctx.headers_sent = false;
	ctx.content_type = alloc_string("text/html");
    r->content_type = val_string(ctx.content_type);

	if( ap_setup_client_block(r,REQUEST_CHUNKED_ERROR) != 0 ) {
		send_headers(&ctx);
		apache_error(APLOG_WARNING,r,"ap_setup_client_block failed");
		return OK;
	}

	ctype = ap_table_get(r->headers_in,"Content-Type");
	if( (!ctype || strstr(ctype,"multipart/form-data") == NULL) && ap_should_client_block(r) ) {
#		define MAXLEN 1024
		char buf[MAXLEN];
		int len;
		int tlen = 0;
		buffer b = alloc_buffer(NULL);
		while( (len = ap_get_client_block(r,buf,MAXLEN)) > 0 ) {
			if( tlen < config.max_post_size )
				buffer_append_sub(b,buf,len);
			tlen += len;
		}
		if( tlen >= config.max_post_size ) {
			send_headers(&ctx);
			apache_error(APLOG_WARNING,r,"Maximum POST data exceeded. Try using multipart encoding");
			return OK;
		}
		ctx.post_data = buffer_to_string(b);
	}

	vm = neko_vm_alloc(NULL);
	if( config.use_stats ) neko_vm_set_stats(vm,neko_stats_measure,config.use_prim_stats?neko_stats_measure:NULL);

	neko_vm_set_custom(vm,k_mod_neko,&ctx);
	if( config.use_jit && !neko_vm_jit(vm,1) ) {
		send_headers(&ctx);
		apache_error(APLOG_WARNING,r,"JIT required by env. var but not enabled in NekoVM");
		return OK;
	}

	neko_vm_redirect(vm,request_print,&ctx);
	neko_vm_select(vm);

	if( ctx.main != NULL ) {
		if( config.use_stats ) neko_stats_measure(vm,r->filename,1);
		val_callEx(val_null,ctx.main,NULL,0,&exc);
		if( config.use_stats ) neko_stats_measure(vm,r->filename,0);
	} else {
		char *base_uri = request_base_uri(r);
		value mload = neko_default_loader(&base_uri,1);
		value args[] = { alloc_string(r->filename), mload };
		char *p = strrchr(val_string(args[0]),'.');
		if( p != NULL )
			*p = 0;
		val_callEx(mload,val_field(mload,val_id("loadmodule")),args,2,&exc);
		if( ctx.main != NULL && config.use_cache )
			cache_module(r,ctx.main);
	}

	if( exc != NULL ) {
		buffer b = alloc_buffer(NULL);
		value v;
		int i;
		const char *p, *start;
		value st = neko_exc_stack(vm);
		val_buffer(b,exc);
		config.exceptions++;
		ap_soft_timeout("Client Timeout",r);
		send_headers(&ctx);
		v = buffer_to_string(b);
		p = val_string(v);
		start = p;
		ap_rprintf(r,"Uncaught exception - ");
		while( *p ) {
			if( *p == '<' || *p == '>' ) {
				ap_rwrite(start,(int)(p - start),r);
				ap_rwrite((*p == '<')?"&lt;":"&gt;",4, r);
				start = p + 1;
			}
			p++;
		}
		ap_rwrite(start,(int)(p - start),r);
		ap_rprintf(r,"<br/><br/>");
		for(i=0;i<val_array_size(st);i++) {
			value s = val_array_ptr(st)[i];
			if( val_is_null(s) )
				ap_rprintf(r,"Called from a C function<br/>");
			else if( val_is_string(s) ) {
				ap_rprintf(r,"Called from %s (no debug available)<br/>",val_string(s));
			} else if( val_is_array(s) && val_array_size(s) == 2 && val_is_string(val_array_ptr(s)[0]) && val_is_int(val_array_ptr(s)[1]) )
				ap_rprintf(r,"Called from %s line %d<br/>",val_string(val_array_ptr(s)[0]),val_int(val_array_ptr(s)[1]));
			else {
				b = alloc_buffer(NULL);
				val_buffer(b,s);
				ap_rprintf(r,"Called from %s<br/>",val_string(buffer_to_string(b)));
			}
		}
		ap_kill_timeout(r);
		return OK;
	}

	send_headers(&ctx);
    return OK;
}

static int neko_handler( request_rec *r ) {
	int ret;
	if( strcmp(r->handler,"neko-handler") != 0)
		return DECLINED;
	if( config.use_stats ) neko_stats_measure(NULL,r->hostname,1);
	ret = neko_handler_rec(r);
	neko_vm_select(NULL);
	if( config.use_stats ) neko_stats_measure(NULL,r->hostname,0);
	gc_major();
	return ret;
}

static void mod_neko_do_init() {
	int tmp = 0;
	if( init_done ) return;
	init_done = 1;
	memset(&config,0,sizeof(config));
	config.use_cache = 1;
	config.gc_period = 1;
	config.max_post_size = MOD_NEKO_POST_SIZE;
#	ifdef APACHE_2_X
	putenv(strdup("MOD_NEKO=2"));
#	else
	putenv(strdup("MOD_NEKO=1"));
#	endif
	cache_root = context_new();	
	neko_global_init(&tmp);
}

static const char *mod_neko_config( cmd_parms *cmd, char *_, const char *args ) {
	int code = 0;
	int value;
	int count = 0;
	while( true ) {
		char c = *args;
		if( c == 0 || c == ' ' || c == '\t' ) break;
		if( count < 4 )	code = (code << 8) | c;
		args++;
		count++;
	}
	while( *args == ' ' || *args == '\t' )
		args++;
	value = atoi(args);
	mod_neko_do_init();
	switch( code ) {
	case 'JIT': config.use_jit = value; break;
	case 'CACH': config.use_cache = value; break;
	case 'GC_P': config.gc_period = value; break;
	case 'POST': config.max_post_size = value; break;
	case 'STAT': config.use_stats = value; break;
	case 'PRIM': config.use_prim_stats = value; break;
	case 'PREL': break;
	default:
		ap_log_error(__FILE__,__LINE__,APLOG_WARNING,LOG_SUCCESS cmd->server,"Unknown ModNeko configuration command %X %X '%s'",code,'JIT',args);
		break;
	}
	return NULL;
}


#ifdef APACHE_2_X
static int neko_init( apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s ) {
	mod_neko_do_init();
	return OK;
}
#else
static void neko_init(server_rec *s, pool *p) {
	mod_neko_do_init();
}
#endif

static command_rec neko_module_cmds[] = {
#	ifdef APACHE_2_X
	AP_INIT_RAW_ARGS( "ModNeko", mod_neko_config , NULL, RSRC_CONF, NULL ),
#	else
	{ "ModNeko", mod_neko_config, NULL, RSRC_CONF, RAW_ARGS, NULL },
#	endif
	{ NULL }
};

#ifdef APACHE_2_X

static void neko_register_hooks( apr_pool_t *p ) {
	ap_hook_post_config( neko_init, NULL, NULL, APR_HOOK_MIDDLE );
	ap_hook_handler( neko_handler, NULL, NULL, APR_HOOK_LAST );
};

module AP_MODULE_DECLARE_DATA neko_module = {
	STANDARD20_MODULE_STUFF,
	NULL,
	NULL,
	NULL,
	NULL,
	neko_module_cmds,
	neko_register_hooks
};

#else /* APACHE 1.3 */

static const handler_rec neko_handlers[] = {
    {"neko-handler", neko_handler},
    {NULL}
};

module MODULE_VAR_EXPORT neko_module = {
    STANDARD_MODULE_STUFF,
    neko_init,
    NULL,
    NULL,
    NULL,
    NULL,
    neko_module_cmds,
    neko_handlers,
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
