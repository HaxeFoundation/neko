/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
/*  Copyright (c)2005 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */
#include "mod_neko.h"

typedef struct cache {
	value file;
	value main;
	time_t time;
	struct cache *next;
} cache;

static _context *cache_root = NULL;

static void send_headers( mcontext *c ) {
	if( !c->headers_sent ) {
		ap_send_http_header(c->r);
		c->headers_sent = true;
	}
}

void request_print( const char *data, int size ) {
	mcontext *c = CONTEXT();
	send_headers(c);
	if( c->allow_write )
		ap_rwrite(data,size,c->r);
}

static value cache_find( request_rec *r ) {
	cache *c = (cache*)context_get(cache_root);
	cache *prev = NULL;
	value fname = alloc_string(r->filename);
	while( c != NULL ) {
		if( val_compare(fname,c->file) == 0 ) {
			if( r->finfo.st_mtime == c->time )
				return c->main;
			if( prev == NULL )
				context_set(cache_root,c->next);
			else
				prev->next = c->next;
			free_root((value*)c);
			break;
		}
		prev = c;
		c = c->next;
	}
	return NULL;
}

static void cache_module( request_rec *r, value main ) {
	cache *c = (cache*)context_get(cache_root);
	value fname = alloc_string(r->filename);
	while( c != NULL ) {
		if( val_compare(fname,c->file) == 0 ) {
			c->main = main;
			c->time = r->finfo.st_mtime;
			return;
		}
		c = c->next;
	}
	c = (cache*)alloc_root(sizeof(struct cache) / sizeof(value));
	c->file = fname;
	c->main = main;
	c->time = r->finfo.st_mtime;
	c->next = (cache*)context_get(cache_root);
	context_set(cache_root,c);
}

static int neko_handler_rec( request_rec *r ) {
	mcontext ctx;
	neko_vm *vm;
	neko_params params;
	value exc = NULL;

	ctx.r = r;
	ctx.main = cache_find(r);
	ctx.post_data = val_null;
	ctx.allow_write = true;
	ctx.headers_sent = false;
    r->content_type = "text/html";

	if( ap_setup_client_block(r,REQUEST_CHUNKED_ERROR) != 0 ) {
		send_headers(&ctx);
		ap_rprintf(r,"<b>Error</b> : ap_setup_client_block failed");
		return OK;
	}

	if( ap_should_client_block(r) ) {
#		define MAXLEN 1024
		char buf[MAXLEN];
		int len;
		buffer b = alloc_buffer(NULL);
		while( (len = ap_get_client_block(r,buf,MAXLEN)) > 0 )
			buffer_append_sub(b,buf,len);
		ctx.post_data = buffer_to_string(b);
	}

	params.custom = &ctx;
	params.printer = request_print;

	{
		request_rec *tmp = r;
		while( tmp->prev != NULL )
			tmp = tmp->prev;
		params.args = &tmp->unparsed_uri;
		params.nargs = 1;
	}

	vm = neko_vm_alloc(&params);
	neko_vm_select(vm);
	
	if( ctx.main != NULL )
		val_callEx(val_null,ctx.main,NULL,0,&exc);
	else {
		value mload = neko_default_loader();
		value args[] = { alloc_string(r->filename), mload };
		char *p = strrchr(val_string(args[0]),'.');
		if( p != NULL )
			*p = 0;
		val_callEx(mload,val_field(mload,val_id("loadmodule")),args,2,&exc);
		if( ctx.main != NULL )
			cache_module(r,ctx.main);		
	}

	if( exc != NULL ) {
		buffer b = alloc_buffer(NULL);
		value v;
		const char *p, *start;
		val_buffer(b,exc);
		send_headers(&ctx);
		v = buffer_to_string(b);
		p = val_string(v);
		start = p;
		ap_rprintf(r,"Uncaught exception - ");
		while( *p ) {
			if( *p == '<' || *p == '>' ) {
				ap_rwrite(start,p - start,r);
				ap_rwrite((*p == '<')?"&lt;":"&gt;",4, r);
				start = p + 1;
			}
			p++;
		}
		ap_rwrite(start,p - start,r);
		return OK;
	}

	send_headers(&ctx);
    return OK;
}

static int neko_handler( request_rec *r ) {
	int ret = neko_handler_rec(r);
	neko_gc_major();
	return ret;
}

static void neko_init(server_rec *s, pool *p) {
	cache_root = context_new();
	putenv("MOD_NEKO=1");
	neko_global_init();
}

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
    NULL,
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

/* ************************************************************************ */
