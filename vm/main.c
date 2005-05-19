/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <stdio.h>
#include "load.h"
#include "interp.h"

static int file_reader( readp p, void *buf, int size ) {
	int len = 0;
	while( size > 0 ) {
		int l = fread(buf,1,size,(FILE*)p);
		if( l <= 0 )
			return len;
		size -= l;
		len += l;
		buf = (char*)buf+l;
	}
	return len;
}

static int execute( neko_vm *vm, char *file ) {
	FILE *f = fopen(file,"rb");
	neko_module *m = NULL;
	if( f == NULL ) {
		printf("File not found %s\n",file);
		return -1;
	}
	m = neko_module_load(file_reader,f);
	fclose(f);
	if( m == NULL ) {
		printf("File %s is not a valid bytecode file\n",file);
		return -2;
	}
	neko_vm_execute(vm,m);
	if( neko_vm_error(vm) ) {
		printf("Error %s\n",neko_vm_error(vm));
		return -3;
	}
	return 0;
}

#include <crtdbg.h>

int main( int argc, char *argv[] ) {
	neko_vm *vm;
	int r;
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	neko_global_init();
	vm = neko_vm_alloc();
	if( argc == 1 ) {
		printf("Usage : nekovm.exe <file>\n");
		r = 1;
	} else
		r = execute(vm,argv[1]);
	neko_vm_free(vm);
	neko_global_free();
	return r;
}

/* ************************************************************************ */
