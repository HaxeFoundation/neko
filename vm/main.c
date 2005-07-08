/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <stdio.h>
#include "load.h"
#include "interp.h"

static int execute( neko_vm *vm, char *file ) {
	value mload = neko_default_loader(NULL);
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
