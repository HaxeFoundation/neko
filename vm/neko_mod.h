/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
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
#ifndef _NEKO_MOD_H
#define _NEKO_MOD_H
#include "neko.h"

typedef struct _neko_module {
	unsigned int nglobals;
	unsigned int nfields;
	unsigned int codesize;
	value name;
	value *globals;
	value *fields;
	value loader;
	value exports;
	value debuginf;
	int_val *code;
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

C_FUNCTION_END

#endif
/* ************************************************************************ */
