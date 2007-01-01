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
#ifdef NEKO_WINDOWS
#	include <windows.h>
#else
#	include <unistd.h>
#endif
#ifdef NEKO_MAC
#	include <sys/param.h>
#	include <mach-o/dyld.h>
#endif
#ifdef NEKO_LINUX
#	include <signal.h>	
#endif

#ifdef NEKO_INSTALLER
extern void neko_installer_error( const char *error );
extern value neko_installer_loader( char *argv[], int argc );
#	define default_loader neko_installer_loader
#else
#	define default_loader neko_default_loader
#endif

static char *data = "##BOOT_POS\0\0\0\0##";
static FILE *self;

int neko_embedded_module() {
	unsigned int data_pos = *(unsigned int*)(data+10);
	if( neko_is_big_endian() )
		data_pos = (data_pos >> 24) | ((data_pos >> 8) & 0xFF00) | ((data_pos << 8) & 0xFF0000) | (data_pos << 24);
	return data_pos;
}

static char *executable_path() {
#if defined(NEKO_WINDOWS)
	static char path[MAX_PATH];
	if( GetModuleFileName(NULL,path,MAX_PATH) == 0 )
		return NULL;
	return path;
#elif defined(NEKO_MAC)
	static char path[MAXPATHLEN+1];
	uint32_t path_len = MAXPATHLEN;
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

static void report( neko_vm *vm, value exc, int isexc ) {
	int i;
	buffer b = alloc_buffer(NULL);
	value st = neko_exc_stack(vm);
	for(i=0;i<val_array_size(st);i++) {
		value s = val_array_ptr(st)[i];
		buffer_append(b,"Called from ");
		if( val_is_null(s) )
			buffer_append(b,"a C function");
		else if( val_is_string(s) ) {
			buffer_append(b,val_string(s));
			buffer_append(b," (no debug available)");
		} else if( val_is_array(s) && val_array_size(s) == 2 && val_is_string(val_array_ptr(s)[0]) && val_is_int(val_array_ptr(s)[1]) ) {
			val_buffer(b,val_array_ptr(s)[0]);
			buffer_append(b," line ");
			val_buffer(b,val_array_ptr(s)[1]);
		} else
			val_buffer(b,s);		
		buffer_append_char(b,'\n');
	}
	if( isexc )
		buffer_append(b,"Uncaught exception - ");
	val_buffer(b,exc);	
#	ifdef NEKO_INSTALLER
	neko_installer_error(val_string(buffer_to_string(b)));
#	else
	fprintf(stderr,"%s\n",val_string(buffer_to_string(b)));
#	endif	
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

int neko_execute_self( neko_vm *vm, value mload ) {
	unsigned int data_pos = neko_embedded_module();
	char *exe = executable_path();
	value args[] = { alloc_string("std@module_read"), alloc_int(2) };
	value args2[] = { alloc_string("std@module_exec"), alloc_int(1) };
	value args3[] = { alloc_function(read_bytecode,3,"boot_read_bytecode"), mload };
	value exc = NULL;
	value module_read, module_exec, module_val;
	if( exe == NULL ) {
		report(vm,alloc_string("Could not resolve current executable name"),0);
		return 1;
	}
	self = fopen("rb",exe);
	if( self == NULL ) {
		report(vm,alloc_string("Failed to open current executable for reading"),0);
		return 1;
	}	
	module_read = val_callEx(mload,val_field(mload,val_id("loadprim")),args,2,&exc);
	if( exc != NULL ) {
		report(vm,exc,1);
		return 1;
	}
	module_exec = val_callEx(mload,val_field(mload,val_id("loadprim")),args2,2,&exc);
	if( exc != NULL ) {
		report(vm,exc,1);
		return 1;
	}
	module_val = val_callEx(val_null,module_read,args3,2,&exc);
	fclose(self);
	if( exc != NULL ) {
		report(vm,exc,1);
		return 1;
	}
	alloc_field(val_field(mload,val_id("cache")),val_id("_self"),module_val);
	val_callEx(val_null,module_exec,&module_val,1,&exc);
	if( exc != NULL ) {
		report(vm,exc,1);
		return 1;
	}
	return 0;
}

static int execute_file( neko_vm *vm, char *file, value mload ) {
	value args[] = { alloc_string(file), mload };
	value exc = NULL;	
	val_callEx(mload,val_field(mload,val_id("loadmodule")),args,2,&exc);
	if( exc != NULL ) {
		report(vm,exc,1);
		return 1;
	}
	return 0;
}

#ifdef NEKO_VCC
#	include <crtdbg.h>
#else
#	define _CrtSetDbgFlag(x)
#endif

#ifdef NEKO_LINUX
static void handle_signal( int signal ) {
	val_throw(alloc_string("Segmentation fault"));
}
#endif

int main( int argc, char *argv[] ) {
	neko_vm *vm;
	value mload;
	int r;
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	neko_global_init(&vm);
	vm = neko_vm_alloc(NULL);
	neko_vm_select(vm);
#	ifdef NEKO_LINUX
	struct sigaction act;
	act.sa_sigaction = NULL;
	act.sa_handler = handle_signal;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGSEGV,&act,NULL);
#	endif
	if( argc > 1 && strcmp(argv[1],"-interp") == 0 ) {
		argc--;
		argv++;
	} else {
		neko_vm_jit(vm,1);
		// ignore error
	}
	if( neko_embedded_module() == 0 ) {
		if( argc == 1 ) {
#			ifdef NEKO_INSTALLER
			report(vm,alloc_string("No embedded module in this executable"),0);
#			else
			printf("NekoVM %d.%d (c)2005-2006 Motion-Twin\n  Usage : neko <file>\n",NEKO_VERSION/100,NEKO_VERSION%100);
#			endif
			mload = NULL;
			r = 1;
		} else {
			mload = default_loader(argv+2,argc-2);
			r = execute_file(vm,argv[1],mload);
		}
	} else {
		mload = default_loader(argv+1,argc-1);
		r = neko_execute_self(vm,mload);
	}
	if( mload != NULL && val_field(mload,val_id("dump_prof")) != val_null )
		val_ocall0(mload,val_id("dump_prof"));
	vm = NULL;
	neko_vm_select(NULL);
	neko_global_free();
	return r;
}

/* ************************************************************************ */
