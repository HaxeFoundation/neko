/*
 * Copyright (C)2005-2012 Haxe Foundation
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
#include <string.h>
#include <neko.h>
#include <neko_mod.h>
#include <neko_vm.h>
#include <stdio.h>

#define READ_BUFSIZE 64

/**
	<doc>
	<h1>Module</h1>
	<p>
	An API for reflexion of Neko bytecode modules.
	</p>
	</doc>
**/

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

/**
	module_read : fread:(buf:string -> pos:int -> len:int -> int) -> loader:object -> 'module
	<doc>
	Read a module using the specified read function and the specified loader.
	</doc>
**/
static value module_read( value fread, value loader ) {
	value p;
	neko_module *m;
	val_check_function(fread,3);
	val_check(loader,object);
	p = alloc_array(2);
	val_array_ptr(p)[0] = fread;
	val_array_ptr(p)[1] = alloc_empty_string(READ_BUFSIZE);
	m = neko_read_module(read_proxy,p,loader);
	if( m == NULL )
		neko_error();
	m->name = alloc_string("");
	return alloc_abstract(neko_kind_module,m);
}

/**
	module_read_string : string -> loader:object -> 'module
	<doc>
	Read a module using the specified string datas.
	</doc>
**/
static value module_read_string( value str, value loader ) {
	neko_module *m;
	string_pos p;
	val_check(str,string);
	val_check(loader,object);
	p.p = val_string(str);
	p.len = val_strlen(str);
	m = neko_read_module(neko_string_reader,&p,loader);
	if( m == NULL )
		neko_error();
	m->name = alloc_string("");
	return alloc_abstract(neko_kind_module,m);
}

/**
	module_read_path : string list -> name:string -> loader:object -> 'module
	<doc>
	Read a module using the specified search path.
	</doc>
**/
static value module_read_path( value path, value name, value loader ) {
	FILE *f;
	value fname;
	char *mname, *ext;
	neko_module *m;
	val_check(name,string);
	val_check(loader,object);
	mname = val_string(name);
	ext = strrchr(mname,'.');
	if( ext && ext[1] == 'n' && ext[2] == 0 )
		fname = neko_select_file(path,mname,"");
	else
		fname = neko_select_file(path,mname,".n");
	f = fopen(val_string(fname),"rb");
	if( f == NULL ) {
		buffer b = alloc_buffer("Module not found : ");
		buffer_append(b,mname);
		bfailure(b);
	}
	m = neko_read_module(neko_file_reader,f,loader);
	fclose(f);
	if( m == NULL ) {
		buffer b = alloc_buffer("Invalid module : ");
		val_buffer(b,name);
		bfailure(b);
	}
	m->name = alloc_string(val_string(name));
	return alloc_abstract(neko_kind_module,m);
}

/**
	module_exec : 'module -> any
	<doc>Execute the module, return the calculated value</doc>
**/
static value module_exec( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return neko_vm_execute(neko_vm_current(),m);
}

/**
	module_name : 'module -> string
	<doc>Return the module name</doc>
**/
static value module_name( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return m->name;
}

/**
	module_set_name : 'module -> string -> void
	<doc>Set the module name</doc>
**/
static value module_set_name( value mv, value str ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	val_check(str,string);
	m = (neko_module*)val_data(mv);
	m->name = str;
	return val_null;
}

/**
	module_exports : 'module -> object
	<doc>Return the module export table</doc>
**/
static value module_exports( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return m->exports;
}

/**
	module_loader : 'module -> object
	<doc>Return the module loader</doc>
**/
static value module_loader( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return m->loader;
}

/**
	module_nglobals : 'module -> int
	<doc>Return the number of globals for this module</doc>
**/
static value module_nglobals( value mv ) {
	neko_module *m;
	val_check_kind(mv,neko_kind_module);
	m = (neko_module*)val_data(mv);
	return alloc_int(m->nglobals);
}

/**
	module_global_get : 'module -> n:int -> any
	<doc>Get the [n]th global</doc>
**/
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

/**
	module_global_set : 'module -> n:int -> any -> void
	<doc>Set the [n]th global</doc>
**/
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

/**
	module_code_size : 'module -> int
	<doc>return the codesize of the module</doc>
**/
static value module_code_size( value mv ) {
	val_check_kind(mv,neko_kind_module);
	return alloc_int( ((neko_module*)val_data(mv))->codesize );
}

DEFINE_PRIM(module_read,2);
DEFINE_PRIM(module_read_string,2);
DEFINE_PRIM(module_read_path,3);
DEFINE_PRIM(module_exec,1);
DEFINE_PRIM(module_name,1);
DEFINE_PRIM(module_exports,1);
DEFINE_PRIM(module_loader,1);
DEFINE_PRIM(module_nglobals,1);
DEFINE_PRIM(module_global_get,2);
DEFINE_PRIM(module_global_set,3);
DEFINE_PRIM(module_code_size,1);
DEFINE_PRIM(module_set_name,2);

/* ************************************************************************ */
