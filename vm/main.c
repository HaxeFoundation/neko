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

#ifdef _WIN32
#	include <crtdbg.h>
#else
#	define _CrtSetDbgFlag(x)
#endif

int main( int argc, char *argv[] ) {
	neko_vm *vm;
	int r;
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	neko_global_init();
	vm = neko_vm_alloc(NULL);
	if( argc == 1 ) {
		printf("Usage : nekovm.exe <file>\n");
		r = 1;
	} else
		r = execute(vm,argv[1]);
	neko_global_free();
	return r;
}

/* ************************************************************************ */
