#ifndef MODNEKO_H
#define MODNEKO_H

#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>
#undef NOERROR
#undef INLINE
#include <context.h>
#include <interp.h>

typedef struct {
	request_rec *r;
	value main;
	value post_data;
	bool headers_sent;
	bool allow_write;
} mcontext;

#define CONTEXT()	((mcontext*)neko_vm_custom(neko_vm_current()))

void request_print( const char *data, int size );

#endif
