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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neko_vm.h"
#ifdef _WIN32
#	include <windows.h>
#else
#	include <unistd.h>
#endif
#ifdef __APPLE__
#	include <sys/param.h>
#	include <mach-o/dyld.h>
#endif

static char *data = "##BOOT_POS\0\0\0\0##";
static FILE *self;

static char *executable_path() {
#ifdef _WIN32
	static char path[MAX_PATH];
	if( GetModuleFileName(NULL,path,MAX_PATH) == 0 )
		return NULL;
	return path;
#elif __APPLE__
	static char path[MAXPATHLEN+1];
	unsigned long path_len = MAXPATHLEN;
	if ( _NSGetExecutablePath(path, &path_len) )
		return NULL;
	return path;
#else
	static char path[200];
	int length = readlink("/proc/self/exe", path, sizeof(path));
	if( length < 0 || length >= 200 )
		return getenv("_");
	path[length] = '\0';
	return path;
#endif
}

static void report( neko_vm *vm, value exc ) {
	int i;
	buffer b;
	value st = neko_exc_stack(vm);
	for(i=0;i<val_array_size(st);i++) {
		value s = val_array_ptr(st)[i];
		if( val_is_null(s) )
			fprintf(stderr,"Called from a C function\n");
		else if( val_is_string(s) ) {
			fprintf(stderr,"Called from %s (no debug available)\n",val_string(s));
		} else if( val_is_array(s) && val_array_size(s) == 2 && val_is_string(val_array_ptr(s)[0]) && val_is_int(val_array_ptr(s)[1]) )
			fprintf(stderr,"Called from %s line %d\n",val_string(val_array_ptr(s)[0]),val_int(val_array_ptr(s)[1]));
		else {
			b = alloc_buffer(NULL);
			val_buffer(b,s);
			fprintf(stderr,"Called from %s\n",val_string(buffer_to_string(b)));
		}
	}
	b = alloc_buffer(NULL);
	val_buffer(b,exc);
	fprintf(stderr,"Uncaught exception - %s\n",val_string(buffer_to_string(b)));
}

static value read_bytecode( value str, value pos, value len ) {
	size_t rlen = fread(val_string(str)+val_int(pos),1,val_int(len),self);
	return alloc_int(rlen);
}

/*
	C functions corresponding to the following Neko code :

	module_read = $loader.loadprim("std@module_read",2);
	module_exec = $loader.loadprim("std@module_exec",1);
	module_val = module_read(read_bytecode,$loader);
	module_exec(module_val);

*/

static int execute_self( neko_vm *vm, value mload ) {	
	value args[] = { alloc_string("std@module_read"), alloc_int(2) };
	value args2[] = { alloc_string("std@module_exec"), alloc_int(1) };
	value args3[] = { alloc_function(read_bytecode,3,"boot_read_bytecode"), mload };	
	value exc = NULL;
	value module_read, module_exec, module_val;
	neko_vm_select(vm);
	module_read = val_callEx(mload,val_field(mload,val_id("loadprim")),args,2,&exc);
	if( exc != NULL ) {
		report(vm,exc);
		return 1;
	}
	module_exec = val_callEx(mload,val_field(mload,val_id("loadprim")),args2,2,&exc);
	if( exc != NULL ) {
		report(vm,exc);
		return 1;
	}
	module_val = val_callEx(val_null,module_read,args3,2,&exc);
	fclose(self);
	if( exc != NULL ) {
		report(vm,exc);
		return 1;
	}
	val_callEx(val_null,module_exec,&module_val,1,&exc);
	if( exc != NULL ) {
		report(vm,exc);
		return 1;
	}
	return 0;
}

static int execute_file( neko_vm *vm, char *file, value mload ) {	
	value args[] = { alloc_string(file), mload };
	value exc = NULL;
	neko_vm_select(vm);
	val_callEx(mload,val_field(mload,val_id("loadmodule")),args,2,&exc);
	if( exc != NULL ) {
		report(vm,exc);
		return 1;
	}
	return 0;
}

#ifdef _MSC_VER
#	include <crtdbg.h>
#else
#	define _CrtSetDbgFlag(x)
#endif

static int execute( neko_vm *vm, char **argv, int argc ) {
	int data_pos = *(int*)(data+10);
	char *exe = executable_path();
	value mload;
	int ret;
	if( neko_is_big_endian() )
		data_pos = (data_pos >> 24) | ((data_pos >> 8) & 0xFF00) | ((data_pos << 8) & 0xFF0000) | (data_pos << 24);	
	if( data_pos == 0 ) {
		if( argc == 1 ) {
			printf("Usage : neko <file>\n");
			return 1;
		} else
			return execute_file(vm,argv[1],neko_default_loader(argv+2,argc-2));
	}
	if( exe == NULL ) {
		printf("Could not resolve current executable\n");
		return 2;
	}
	self = fopen(exe,"rb");
	if( self == NULL ) {
		printf("Could not open current executable for reading\n");
		return 2;
	}
	fseek(self,data_pos,0);
	mload = neko_default_loader(argv+1,argc-1);
	ret = execute_self(vm,mload);
	if( val_field(mload,val_id("dump_prof")) != val_null )
		val_ocall0(mload,val_id("dump_prof"));
	return ret;
}

int main( int argc, char *argv[] ) {
	neko_vm *vm;
	neko_params p;
	int r;
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	neko_global_init(&vm);
	p.custom = NULL;
	p.printer = NULL;
	vm = neko_vm_alloc(&p);
	r = execute(vm,argv,argc);
	vm = NULL;
	neko_global_free();
	return r;
}

/* ************************************************************************ */
