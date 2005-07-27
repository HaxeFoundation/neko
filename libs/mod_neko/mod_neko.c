#define neko_module neko_mod
#include <neko.h>
#include <load.h>
#include "mod_neko.h"
#undef neko_module

#ifdef _WIN32
long _ftol( double f );
long _ftol2( double f) { return _ftol(f); };
#endif

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
		ap_rputs(data,c->r);
}

static int neko_handler(request_rec *r) {
	mcontext ctx;
	neko_vm *vm;
	neko_params params;

    r->content_type = "text/html";

	ctx.r = r;
	ctx.main = NULL;
	ctx.post_data = val_null;
	ctx.allow_write = true;
	ctx.headers_sent = false;

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

	vm = neko_vm_alloc(&params);
	neko_vm_select(vm);
	{
		value mload = neko_default_loader(NULL);
		value args[] = { alloc_string(r->filename), mload };
		value exc = NULL;
		char *p = strrchr(val_string(args[0]),'.');
		if( p != NULL )
			*p = 0;
		val_callEx(mload,val_field(mload,val_id("loadmodule")),args,2,&exc);
		if( exc != NULL ) {
			buffer b = alloc_buffer(NULL);
			val_buffer(b,exc);
			send_headers(&ctx);
			ap_rprintf(r,"Uncaught exception - %s\n",val_string(buffer_to_string(b)));
			return OK;
		}
	}

	send_headers(&ctx);
    return OK;
}

static void neko_init(server_rec *s, pool *p) {
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
