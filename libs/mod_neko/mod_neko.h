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
#include <neko_vm.h>
#ifndef NEKO_WINDOWS
#	include <arpa/inet.h>
#endif

typedef struct {
	request_rec *r;
	value main;
	value post_data;
	value content_type;
	bool headers_sent;
} mcontext;

typedef struct {
	int hits;
	int use_jit;
	int use_stats;
	int use_prim_stats;
	int use_cache;
	int exceptions;
	int gc_period;
	int max_post_size;
} mconfig;

#define CONTEXT()	((mcontext*)neko_vm_custom(neko_vm_current(),k_mod_neko))

DECLARE_KIND(k_mod_neko)

#ifdef STANDARD20_MODULE_STUFF
#	define APACHE_2_X
#	define REMOTE_ADDR(c)	c->remote_addr->sa.sin.sin_addr
#else
#	define REMOTE_ADDR(c)	c->remote_addr.sin_addr
#endif

extern mconfig *mod_neko_get_config();
extern void mod_neko_set_config( mconfig *cfg );

#endif

/* ************************************************************************ */
