/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include "neko.h"
#include "objtable.h"
#include "vmcontext.h"

typedef value (*c_prim0)();
typedef value (*c_prim1)(value);
typedef value (*c_prim2)(value,value);
typedef value (*c_prim3)(value,value,value);
typedef value (*c_prim4)(value,value,value,value);
typedef value (*c_primN)(value*,int);

EXTERN value val_callN( value f, value *args, int nargs ) {
	if( !val_is_function(f) || nargs < 0 )
		return val_null;
	if( val_tag(f) == VAL_PRIMITIVE ) {
		neko_vm *vm = NEKO_VM();
		value v;
		value old_env = vm->env;
		vm->env = ((vfunction *)f)->env;
		if( nargs == ((vfunction*)f)->nargs ) {
			switch( nargs ) {
			case 0:
				v = ((c_prim0)((vfunction*)f)->addr)();
				break;
			case 1:
				v = ((c_prim1)((vfunction*)f)->addr)(args[0]);
				break;
			case 2:
				v = ((c_prim2)((vfunction*)f)->addr)(args[0],args[1]);
				break;
			case 3:
				v = ((c_prim3)((vfunction*)f)->addr)(args[0],args[1],args[2]);
				break;
			case 4:
				v = ((c_prim4)((vfunction*)f)->addr)(args[0],args[1],args[2],args[3]);
				break;
			default:
				v = val_null;
				break;
			}
		} else if( ((vfunction*)f)->nargs == -1 )
			v = (value)((c_primN)((vfunction*)f)->addr)(args,nargs);
		if( v == NULL )
			v = val_null;
		vm->env = old_env;
		return v;
	} else if( val_tag(f) == VAL_FUNCTION ) {
		if( nargs == ((vfunction*)f)->nargs )  {
			neko_vm *vm = NEKO_VM();
			int n;
			value v;
			if( vm->csp + 3 >= vm->sp - nargs ) {
				vm->error = "OVERFLOW";
				return val_null;
			}
			for(n=0;n<nargs;n++)
				*--vm->sp = (unsigned int)args[n];
			*++vm->csp = (int)callback_return;
			*++vm->csp = 0;
			*++vm->csp = (int)vm->val_this;
			v = interp(vm,(int)val_null,(int*)((vfunction*)f)->addr,((vfunction*)f)->env);
			if( vm->error != NULL )
				return val_null;
			return v;
		}
	}
	return val_null;
}

EXTERN value val_ocallN( value o, field f, value *args, int nargs ) {
	neko_vm *vm = NEKO_VM();
	value *meth = otable_find(((vobject*)o)->table,f);
	value old_this = vm->val_this;
	value r;
	if( meth == NULL )
		return val_null;
	vm->val_this = o;
	r = val_callN(*meth,args,nargs);
	vm->val_this = old_this;
	return r;
}

EXTERN value val_call0( value f ) {
	return val_callN(f,NULL,0);
}

EXTERN value val_call1( value f, value v ) {
	return val_callN(f,&v,1);
}

EXTERN value val_call2( value f, value v1, value v2 ) {
	value args[2] = { v1, v2 };
	return val_callN(f,args,2);
}

EXTERN value val_ocall0( value o, field f ) {
	return val_ocallN(o,f,NULL,0);
}

EXTERN value val_ocall1( value o, field f, value arg ) {
	return val_ocallN(o,f,&arg,1);
}

EXTERN value val_this() {
	return (value)NEKO_VM()->val_this;
}

/* ************************************************************************ */
