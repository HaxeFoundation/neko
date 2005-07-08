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

static int load_module( const char *mname, reader *r, readp *p ) {
	FILE *f = fopen(mname,"rb");
	if( f == NULL )
		return 0;
	*r = file_reader;
	*p = f;
	return 1;
}

static void load_done( readp p ) {
	fclose(p);
}

static int execute( neko_vm *vm, char *file ) {
	loader l;
	value mload;
	l.l = load_module;
	l.d = load_done;
	mload = neko_default_loader(&l);
	{
		value args[] = { alloc_string(file), mload };
		value exc = NULL;
		neko_vm_select(vm);
		val_callEx(mload,val_field(mload,val_id("loadmodule")),args,2,&exc);
		if( exc != NULL ) {
			buffer b = alloc_buffer(NULL);
			val_buffer(b,exc);
			printf("Uncaught exception - %s\n",val_string(buffer_to_string(b)));
			return 1;
		}
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
