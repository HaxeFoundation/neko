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
#ifdef NEKO_POSIX
#	include <signal.h>
#endif

#ifdef NEKO_STANDALONE
	extern void neko_standalone_init();
	extern void neko_standalone_error( const char *str );
	extern value neko_standalone_loader( char **arv, int argc );
#	define default_loader neko_standalone_loader
#else
#	define default_loader neko_default_loader
#endif
static FILE *self;

extern void neko_stats_measure( neko_vm *vm, const char *kind, int start );
extern value neko_stats_build( neko_vm *vm );


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
	if( length < 0 || length >= 200 ) {
		char *p = getenv("   "); // for upx
		if( p == NULL )
			p = getenv("_");
		return p;
	}
	path[length] = '\0';
	return path;
#endif
}

int neko_has_embedded_module( neko_vm *vm ) {
	char *exe = executable_path();
	unsigned char id[8];
	int pos;
	if( exe == NULL )
		return 0;
	self = fopen(exe,"rb");
	if( self == NULL )
		return 0;
	fseek(self,-8,SEEK_END);
	if( fread(id,1,8,self) != 8 || id[0] != 'N' || id[1] != 'E' || id[2] != 'K' || id[3] != 'O' ) {
		fclose(self);
		return 0;
	}
	pos = id[4] | id[5] << 8 | id[6] << 16;
	fseek(self,pos,SEEK_SET);
	// flags
	if( (id[7] & 1) == 0 )
		neko_vm_jit(vm,1);
	return 1;
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
#	ifdef NEKO_STANDALONE
	neko_standalone_error(val_string(buffer_to_string(b)));
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
	value args[] = { alloc_string("std@module_read"), alloc_int(2) };
	value args2[] = { alloc_string("std@module_exec"), alloc_int(1) };
	value args3[] = { alloc_function(read_bytecode,3,"boot_read_bytecode"), mload };
	value exc = NULL;
	value module_read, module_exec, module_val;
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

#ifdef NEKO_POSIX
static void handle_signal( int signal ) {
	val_throw(alloc_string("Segmentation fault"));
}
#endif

int main( int argc, char *argv[] ) {
	neko_vm *vm;
	value mload;
	int r;
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	neko_global_init();
	vm = neko_vm_alloc(NULL);
	neko_vm_select(vm);
#	ifdef NEKO_STANDALONE
	neko_standalone_init();
#	endif
	if( !neko_has_embedded_module(vm) ) {
		int jit = 1;
		int stats = 0;
		while( argc > 1 ) {
			if( strcmp(argv[1],"-interp") == 0 ) {
				argc--;
				argv++;
				jit = 0;
				continue;
			}
			if( strcmp(argv[1],"-stats") == 0 ) {
				argc--;
				argv++;
				stats = 1;
				neko_vm_set_stats(vm,neko_stats_measure,neko_stats_measure);
				neko_stats_measure(vm,"total",1);
				continue;
			}
			if( strcmp(argv[1],"-version") == 0 ) {
				argc--;
				argv++;
				printf("%d.%d.%d\n",NEKO_VERSION/100,(NEKO_VERSION/10)%10,NEKO_VERSION%10);
				return 0;
			}
			break;
		}
#		ifdef NEKO_POSIX
		if( jit ) {
			struct sigaction act;
			act.sa_sigaction = NULL;
			act.sa_handler = handle_signal;
			act.sa_flags = 0;
			sigemptyset(&act.sa_mask);
			sigaction(SIGSEGV,&act,NULL);
		}
#		endif
		neko_vm_jit(vm,jit);
		if( argc == 1 ) {
#			ifdef NEKO_STANDALONE
			report(vm,alloc_string("No embedded module in this executable"),0);
#			else
			printf("NekoVM %d.%d.%d (c)2005-2013 Haxe Foundation\n  Usage : neko <file>\n",NEKO_VERSION/100,(NEKO_VERSION/10)%10,NEKO_VERSION%10);
#			endif
			mload = NULL;
			r = 1;
		} else {
			mload = default_loader(argv+2,argc-2);
			r = execute_file(vm,argv[1],mload);
		}
		if( stats ) {
			value v;
			neko_stats_measure(vm,"total",0);
			v = neko_stats_build(vm);
			val_print(alloc_string("TOT\tTIME\tCOUNT\tNAME\n"));
			while( v != val_null ) {
				char buf[256];
				value *s = val_array_ptr(v);
				int errors = val_int(s[4]);
				sprintf(buf,"%d\t%d\t%d\t%s%c",
					val_int(s[1]),
					val_int(s[2]),
					val_int(s[3]),
					val_string(s[0]),
					errors?' ':'\n');
				if( errors )
					sprintf(buf+strlen(buf),"ERRORS=%d\n",errors);
				val_print(alloc_string(buf));
				v = s[5];
			}
		}
	} else {
		mload = default_loader(argv+1,argc-1);
		r = neko_execute_self(vm,mload);
	}
	if( mload != NULL && val_field(mload,val_id("dump_prof")) != val_null )
		val_ocall0(mload,val_id("dump_prof"));
	vm = NULL;
	mload = NULL;
	neko_vm_select(NULL);
	neko_global_free();
	return r;
}

/* ************************************************************************ */
