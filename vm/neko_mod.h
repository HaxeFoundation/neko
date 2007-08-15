/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
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
#ifndef _NEKO_MOD_H
#define _NEKO_MOD_H
#include "neko.h"

typedef struct _neko_debug {
	int base;
	unsigned int bits;
} neko_debug;

typedef struct _neko_module {
	void *jit;
	unsigned int nglobals;
	unsigned int nfields;
	unsigned int codesize;
	value name;
	value *globals;
	value *fields;
	value loader;
	value exports;
	value dbgtbl;
	neko_debug *dbgidxs;
	int_val *code;
	value jit_gc;
} neko_module;

typedef void *readp;
typedef int (*reader)( readp p, void *buf, int size );

typedef struct {
	char *p;
	int len;
} string_pos;

C_FUNCTION_BEGIN

EXTERN field neko_id_module;
EXTERN vkind neko_kind_module;
EXTERN neko_module *neko_read_module( reader r, readp p, value loader );
EXTERN int neko_file_reader( readp p, void *buf, int size ); // FILE *
EXTERN int neko_string_reader( readp p, void *buf, int size ); // string_pos *
EXTERN value neko_select_file( value path, const char *file, const char *ext );

C_FUNCTION_END

#endif
/* ************************************************************************ */
