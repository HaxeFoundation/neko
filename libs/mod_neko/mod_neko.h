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
#include <neko_vm.h>

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
#endif

extern mconfig *mod_neko_get_config();
extern void mod_neko_set_config( mconfig *cfg );

#endif

/* ************************************************************************ */
