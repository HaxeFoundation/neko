/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <math.h>
#include <string.h>
#include <stdio.h>
#undef NULL
#include "context.h"
#include "interp.h"
#include "load.h"
#include "opcodes.h"
#include "vmcontext.h"
#include "objtable.h"

#define INIT_STACK_SIZE (1 << 10)
#define MAX_STACK_SIZE	(1 << 18)

_context *vm_context = NULL;
value interp( neko_vm *vm, int *code, value env );

static void default_printer( const char *s, int len ) {
	while( len > 0 ) {
		int p = fwrite(s,1,len,stdout);
		if( p <= 0 ) {
			fputs("[ABORTED]",stdout);
			break;
		}
		len -= p;
		s += p;
	}
	fputc('\n',stdout);
}

neko_vm *neko_vm_alloc() {
	int i;
	int *p;
	neko_vm *vm = (neko_vm*)alloc_abstract(sizeof(neko_vm));
	vm->spmin = (int*)alloc_root(INIT_STACK_SIZE);
	p = vm->spmin;
	for(i=0;i<STACK_DELTA;i++)
		*p++ = (int)val_null;
	vm->print = default_printer;
	vm->spmax = vm->spmin + INIT_STACK_SIZE;
	vm->sp = vm->spmax;
	vm->csp = p - 1;
	vm->val_this = val_null;
	return vm;
}

void neko_vm_free( neko_vm *vm ) {
	free_root((value*)vm->spmin);
}

void neko_vm_select( neko_vm *vm ) {
	context_set(vm_context,vm);
}

void neko_vm_execute( neko_vm *vm, neko_module *m ) {
	neko_vm_select(vm);
	interp(vm,m->code,alloc_array(0));
}

const char*neko_vm_error( neko_vm *vm ) {
	return vm->error;
}

static int stack_expand( int *sp, int *csp, neko_vm *vm ) {
	int i;
	int size = (((int)vm->spmax - (int)vm->spmin) / sizeof(int)) << 1;
	int *nsp;
	if( size > MAX_STACK_SIZE )
		return 0;
	nsp = (int*)alloc_root(size);
	
	// csp size
	i = ((int)(csp + 1) - (int)vm->spmin) / sizeof(int);
	memcpy(nsp,vm->spmin,sizeof(int) * i);
	vm->csp = nsp + i - 1;
	
	i = ((int)vm->spmax - (int)sp) / sizeof(int);
	memcpy(nsp+size-i,sp,sizeof(int) * i);
	vm->sp = nsp + size - i;

	free_root((value*)vm->spmin);
	vm->spmin = nsp;
	vm->spmax = nsp + size;
	return 1;
}

typedef int (*c_prim0)();
typedef int (*c_prim1)(int);
typedef int (*c_prim2)(int,int);
typedef int (*c_prim3)(int,int,int);
typedef int (*c_prim4)(int,int,int,int);
typedef int (*c_primN)(value*,int);


#define DynError(cond,err)		if( cond ) { vm->error = #err; return val_null; }
#define Error(cond,err)			DynError(cond,err)
#define Instr(x)	case x:
#define Next		break;

#define PopMacro(n) \
		tmp = n; \
		Error(sp + tmp > vm->spmax, UNDERFLOW); \
		while( tmp-- > 0 ) \
			*sp++ = NULL;

#define SetupBeforeCall(this_arg) \
		value old_this = vm->val_this; \
		vm->val_this = this_arg; \
		vm->sp = sp; \
		vm->csp = csp;

#define RestoreAfterCall() \
		if( acc == NULL ) acc = (int)val_null; \
		vm->val_this = old_this; \
		if( vm->error ) \
			return val_null;

#define DoCall(this_arg) \
		if( acc & 1 ) { \
			acc = (int)val_null; \
			PopMacro(*pc++); \
		} else if( val_tag(acc) == VAL_FUNCTION && *pc == ((vfunction*)acc)->nargs ) { \
			if( csp + 3 >= sp ) { \
				DynError( !stack_expand(sp,csp,vm) , OVERFLOW ); \
				sp = vm->sp; \
				csp = vm->csp; \
			} \
			*++csp = (int)(pc+1); \
			*++csp = (int)env; \
			*++csp = (int)vm->val_this; \
			pc = (int*)((vfunction*)acc)->addr; \
			vm->val_this = this_arg; \
			env = ((vfunction*)acc)->env; \
		} else if( val_tag(acc) == VAL_PRIMITIVE ) { \
			Error( sp + *pc > vm->spmax , UNDERFLOW ); \
			if( *pc == ((vfunction*)acc)->nargs ) { \
				SetupBeforeCall(this_arg); \
				switch( *pc ) { \
				case 0: \
					acc = ((c_prim0)((vfunction*)acc)->addr)(); \
					break; \
				case 1: \
					acc = ((c_prim1)((vfunction*)acc)->addr)(sp[0]); \
					break; \
				case 2: \
					acc = ((c_prim2)((vfunction*)acc)->addr)(sp[1],sp[0]); \
					break; \
				case 3: \
					acc = ((c_prim3)((vfunction*)acc)->addr)(sp[2],sp[1],sp[0]); \
					break; \
				case 4: \
					acc = ((c_prim4)((vfunction*)acc)->addr)(sp[3],sp[2],sp[1],sp[0]); \
					break; \
				} \
				RestoreAfterCall(); \
			} \
			else if( ((vfunction*)acc)->nargs == -1 ) { \
				int args[CALL_MAX_ARGS]; \
				SetupBeforeCall(this_arg); \
				sp += *pc; \
				for(tmp=0;tmp<*pc;tmp++) \
					args[tmp] = *--sp; \
				acc = ((c_primN)((vfunction*)acc)->addr)((value*)args,*pc); \
				RestoreAfterCall(); \
			} else \
				acc = (int)val_null; \
			PopMacro(*pc++); \
		} else { \
			acc = (int)val_null; \
			PopMacro(*pc++); \
		}

#define IntOp(op) \
		Error( sp == vm->spmax , UNDERFLOW ); \
		if( (acc & 1) && (*sp & 1) ) \
			acc = (int)alloc_int(val_int(*sp) op val_int(acc)); \
		else \
			acc = (int)val_null; \
		*sp++ = NULL; \
		Next

#define Test(test) \
		Error( sp == vm->spmax , UNDERFLOW ); \
		acc = (int)val_compare((value)*sp,(value)acc); \
		*sp++ = NULL; \
		acc = (int)((acc test 0)?val_true:val_false); \
		Next

#define SUB(x,y) ((x) - (y))
#define MULT(x,y) ((x) - (y))
#define DIV(x,y) ((x) / (y))

#define NumberOp(op,fop) \
		Error( sp == vm->spmax , UNDERFLOW ); \
		if( (acc & 1) && (*sp & 1) ) \
			acc = (int)alloc_int(val_int(*sp) op val_int(acc)); \
		else if( acc & 1 ) { \
			if( val_tag(*sp) == VAL_FLOAT ) \
				acc = (int)alloc_float(fop(val_float(*sp),val_int(acc))); \
			else \
				acc = (int)val_null; \
		} else if( *sp & 1 ) { \
			if( val_tag(acc) == VAL_FLOAT ) \
				acc = (int)alloc_float(fop(val_int(*sp),val_float(acc))); \
			else \
				acc = (int)val_null; \
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT ) \
			acc = (int)alloc_float(fop(val_float(*sp),val_float(acc))); \
		else \
			acc = (int)val_null; \
		*sp++ = NULL; \
		Next;

#define AppendString(str,fmt,x,way) { \
		int len, len2; \
		value v; \
		len = val_strlen(str); \
		len2 = sprintf(vm->tmp,fmt,x); \
		v = alloc_empty_string(len+len2); \
		if( way ) { \
		 	memcpy((char*)val_string(v),val_string(str),len); \
			memcpy((char*)val_string(v)+len,vm->tmp,len2+1); \
		} else { \
			memcpy((char*)val_string(v),vm->tmp,len2); \
			memcpy((char*)val_string(v)+len2,val_string(str),len+1); \
		} \
		acc = (int)v; \
	}

value interp( neko_vm *vm, register int *pc, value env ) {
	register int acc = (int)val_null;
	register int *sp = vm->sp;
	register int *csp = vm->csp;
	int tmp;
	while( true ) {
		switch( *pc++ ) {
	Instr(AccNull)
		acc = (int)val_null;
		Next;
	Instr(AccTrue)
		acc = (int)val_true;
		Next;
	Instr(AccFalse)
		acc = (int)val_false;
		Next;
	Instr(AccThis)
		acc = (int)vm->val_this;
		Next;
	Instr(AccInt)
		acc = *pc++;
		Next;
	Instr(AccStackFast)
		acc = sp[*pc++];
		Next;
	Instr(AccStack)
		Error( sp + *pc >= vm->spmax , OUT_STACK );
		acc = sp[*pc++];
		Next;
	Instr(AccGlobal)
		acc = *(int*)(*pc++);
		Next;
	Instr(AccEnv)
		Error( *pc >= val_array_size(env) , OUT_ENV );
		acc = (int)val_array_ptr(env)[*pc++];
		Next;
	Instr(AccField)
		if( val_is_object(acc) ) {
			value *f = otable_find(((vobject*)acc)->table,(field)*pc);
			acc = (int)(f?*f:val_null);
		} else
			acc = (int)val_null;
		pc++;
		Next;
	Instr(AccArray)
		Error( sp == vm->spmax , UNDERFLOW );
		if( val_is_int(acc) && val_is_array(*sp) ) {
			int k = val_int(acc);
			if( k < 0 || k >= val_array_size(*sp) )
				acc = (int)val_null;
			else
				acc = (int)val_array_ptr(*sp)[k];
		} else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(AccBuiltin)
		acc = *pc++;
		Next;
	Instr(SetStackFast)
		sp[*pc++] = acc;
		Next;
	Instr(SetStack)
		Error( sp + *pc >= vm->spmax , OUT_STACK );
		sp[*pc++] = acc;
		Next;
	Instr(SetGlobal)
		*(int*)(*pc++) = acc;
		Next;
	Instr(SetEnv)
		Error( *pc >= val_array_size(env) , OUT_ENV );
		val_array_ptr(env)[*pc++] = (value)acc;
		Next;
	Instr(SetField)
		if( val_is_object(acc) )
			otable_replace(((vobject*)acc)->table,(field)*pc,(value)acc);
		pc++;
		Next;
	Instr(SetArray)
		Error( sp == vm->spmax , UNDERFLOW);
		if( val_is_int(acc) && val_is_array(*sp) ) {
			int k = val_int(acc);
			if( k >= 0 && k < val_array_size(*sp) )
				val_array_ptr(*sp)[k] = (value)acc;
		}
		*sp++ = NULL;
		Next;
	Instr(Push)
		--sp;
		if( sp <= csp ) {
			DynError( !stack_expand(sp,csp,vm) , OVERFLOW );
			sp = vm->sp;
			csp = vm->csp;
		}
		*sp = acc;
		Next;
	Instr(Pop)
		PopMacro(*pc++)
		Next;
	Instr(Call)
		DoCall(vm->val_this);
		Next;
	Instr(ObjCall)
		Error( sp == vm->spmax , UNDERFLOW );
		tmp = *sp;
		*sp++ = NULL;
		DoCall((value)tmp);
		Next;
	Instr(Jump)
		pc = (int*)*pc;
		Next;
	Instr(JumpIf)
		if( acc == (int)val_true )
			pc = (int*)*pc;
		else
			pc++;
		Next;
	Instr(JumpIfNot)
		if( acc != (int)val_true )
			pc = (int*)*pc;
		else
			pc++;
		Next;
/*
Trap,
EndTrap,
*/
	Instr(Ret)
		Error( csp - 2 < vm->spmin , UNDERFLOW );
		PopMacro( *pc++ );
		vm->val_this = (value)*csp;
		*csp-- = NULL;
		env = (value)*csp;
		*csp-- = NULL;
		pc = (int*)*csp;
		*csp-- = NULL;
		Next;
	Instr(MakeEnv)
		{
			int n = *pc++;
			tmp = (int)alloc_array(n);
			Error( sp + n > vm->spmax , UNDERFLOW);
			while( n-- ) {
				val_array_ptr(tmp)[n] = (value)*sp;
				*sp++ = NULL;
			}
			if( !val_is_int(acc) && val_tag(acc) == VAL_FUNCTION ) {
				acc = (int)alloc_function(((vfunction*)acc)->addr,((vfunction*)acc)->nargs);
				((vfunction*)acc)->env = (value)tmp;
			} else
				acc = (int)val_null;
		}
		Next;
	Instr(Bool)
		acc = (acc == (int)val_false || acc == (int)val_null || acc == 1)?(int)val_false:(int)val_true;
		Next;
	Instr(Add)
		Error( sp == vm->spmax , UNDERFLOW );
		if( (acc & 1) && (*sp & 1) )
			acc = (int)alloc_int(val_int(*sp) + val_int(acc));
		else if( acc & 1 ) {
			if( val_tag(*sp) == VAL_FLOAT )
				acc = (int)alloc_float(val_float(*sp) + val_int(acc));
			else if( (val_tag(*sp)&7) == VAL_STRING )
				AppendString(*sp,"%d",val_int(acc),true)
			else
				acc = (int)val_null;
		} else if( *sp & 1 ) {
			if( val_tag(acc) == VAL_FLOAT )
				acc = (int)alloc_float(val_int(*sp) + val_float(acc));
			else if( (val_tag(acc)&7) == VAL_STRING )
				AppendString(acc,"%d",val_int(*sp),false)
			else
				acc = (int)val_null;
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT )
			acc = (int)alloc_float(val_float(*sp) + val_float(acc));
		else if( (tmp = val_tag(acc)&7) == VAL_STRING && (val_tag(*sp)&7) == VAL_STRING ) {
			int len1 = val_strlen(*sp);
			int len2 = val_strlen(acc);
			value v = alloc_empty_string(len1+len2);
			memcpy((char*)val_string(v),val_string(*sp),len1);
			memcpy((char*)val_string(v)+len1,val_string(acc),len2+1);
			acc = (int)v;
		} else if( tmp == VAL_STRING || (val_tag(*sp)&7) == VAL_STRING ) {
			buffer b = alloc_buffer(NULL);
			val_buffer(b,(value)*sp);
			val_buffer(b,(value)acc);
			acc = (int)buffer_to_string(b);
		} else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(Sub)
		NumberOp(-,SUB)
	Instr(Mult)
		NumberOp(*,MULT)
	Instr(Div)
		if( acc == 1 ) { // division by integer 0
			Error( sp == vm->spmax , UNDERFLOW );
			acc	= (int)val_null;
			*sp++ = NULL;
			Next;
		}
		NumberOp(/,DIV);
	Instr(Mod)
		if( acc == 1 ) {
			Error( sp == vm->spmax , UNDERFLOW );
			acc	= (int)val_null;
			*sp++ = NULL;
			Next;
		}
		NumberOp(%,fmod);
	Instr(Shl)
		IntOp(<<);
	Instr(Shr)
		IntOp(>>);
	Instr(UShr)
		Error( sp == vm->spmax , UNDERFLOW);
		if( (acc & 1) && (*sp & 1) )
			acc = (int)alloc_int(((unsigned int)val_int(*sp)) >> val_int(acc));
		else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(Or)
		IntOp(|);
	Instr(And)
		IntOp(&);
	Instr(Xor)
		IntOp(^);
	Instr(Eq)
		Test(==)
	Instr(Neq)
		Test(!=)
	Instr(Lt)
		Test(<)
	Instr(Lte)
		Test(<=)
	Instr(Gt)
		Test(>)
	Instr(Gte)
		Test(>=)
	Instr(Last)
		return (value)acc;
	}}
}

/* ************************************************************************ */
