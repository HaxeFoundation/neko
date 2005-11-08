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
#include <string.h>
#include <neko.h>
#include <neko_mod.h>
#include <neko_vm.h>

#define READ_BUFSIZE 64

static int read_proxy( readp p, void *buf, int size ) {
	value fread = val_array_ptr(p)[0];
	value vbuf = val_array_ptr(p)[1];
	value ret;
	int len;
	if( size < 0 )
		return -1;
	if( size > READ_BUFSIZE )
		vbuf = alloc_empty_string(size);
	ret = val_call3(fread,vbuf,alloc_int(0),alloc_int(size));
	if( !val_is_int(ret) )
		return -1;
	len = val_int(ret);
	if( len < 0 || len > size )
		return -1;
	memcpy(buf,val_string(vbuf),len);
	return len;
}

static value module_read( value fread, value loader ) {
	value p;
	neko_module *m;
	val_check_function(fread,3);
	p = alloc_array(2);
	val_array_ptr(p)[0] = fread;
	val_array_ptr(p)[1] = alloc_empty_string(READ_BUFSIZE);
	m = neko_read_module(read_proxy,p,loader);
	m->name = alloc_string("");
	if( m == NULL )
		neko_error();
	return alloc_abstract(neko_kind_module,m);
}

static value module_exec( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return neko_vm_execute(neko_vm_current(),m);
}

static value module_name( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return m->name;
}

static value module_exports( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return m->exports;
}

static value module_loader( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return m->loader;
}

static value module_nglobals( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return alloc_int(m->nglobals);
}

static value module_global_get( value mv, value p ) {
	neko_module *m;
	unsigned int pp;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	val_check(p,int);
	pp = (unsigned)val_int(p);
	if( pp >= m->nglobals )
		neko_error();
	return m->globals[pp];
}

static value module_global_set( value mv, value p, value v ) {
	neko_module *m;
	unsigned int pp;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	val_check(p,int);
	pp = (unsigned)val_int(p);
	if( pp >= m->nglobals )
		neko_error();
	m->globals[pp] = v;
	return v;
}

DEFINE_PRIM(module_read,2);
DEFINE_PRIM(module_exec,1);
DEFINE_PRIM(module_name,1);
DEFINE_PRIM(module_exports,1);
DEFINE_PRIM(module_loader,1);
DEFINE_PRIM(module_nglobals,1);
DEFINE_PRIM(module_global_get,2);
DEFINE_PRIM(module_global_set,3);

/* ************************************************************************ */
