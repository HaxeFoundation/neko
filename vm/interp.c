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

#define INIT_STACK_SIZE (1 << 7)
#define MAX_STACK_SIZE	(1 << 18)

#define address_int(a)	(((int)(a)) | 1)
#define int_address(a)	(int*)(a & ~1)

extern field id_add;
extern field id_preadd;
extern value alloc_module_function( void *m, int pos, int nargs );

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

EXTERN neko_vm *neko_vm_alloc( neko_params *p ) {
	int i;
	neko_vm *vm = (neko_vm*)alloc(sizeof(neko_vm));	
	vm->spmin = (int*)alloc(INIT_STACK_SIZE*sizeof(int));
	vm->print = (p && p->printer)?p->printer:default_printer;
	vm->custom = p?p->custom:NULL;
	vm->spmax = vm->spmin + INIT_STACK_SIZE;
	vm->sp = vm->spmax;
	for(i=0;i<STACK_DELTA;i++)
		*--vm->sp = (int)val_null;
	vm->csp = vm->spmin - 1;
	vm->this = val_null;
	vm->env = alloc_array(0);
	return vm;
}

EXTERN void neko_vm_select( neko_vm *vm ) {
	context_set(neko_vm_context,vm);
}

EXTERN neko_vm *neko_vm_current() {
	return (neko_vm*)context_get(neko_vm_context);
}

EXTERN void *neko_vm_custom( neko_vm *vm ) {
	return vm->custom;
}

EXTERN void neko_vm_execute( neko_vm *vm, neko_module *m ) {
	unsigned int i;
	neko_vm_select(vm);
	for(i=0;i<m->nfields;i++)
		val_id(val_string(m->fields[i]));
	neko_interp(vm,(int)val_null,m->code,alloc_array(0));
}

int neko_stack_expand( int *sp, int *csp, neko_vm *vm ) {
	int i;
	int size = (((int)vm->spmax - (int)vm->spmin) / sizeof(int)) << 1;
	int *nsp;
	if( size > MAX_STACK_SIZE )
		return 0;
	nsp = (int*)alloc(size * sizeof(int));
	
	// csp size
	i = ((int)(csp + 1) - (int)vm->spmin) / sizeof(int);
	memcpy(nsp,vm->spmin,sizeof(int) * i);
	vm->csp = nsp + i - 1;
	
	// sp size
	i = ((int)vm->spmax - (int)sp) / sizeof(int);
	memcpy(nsp+size-i,sp,sizeof(int) * i);
	vm->sp = nsp + size - i;
	vm->spmin = nsp;
	vm->spmax = nsp + size;
	return 1;
}

typedef int (*c_prim0)();
typedef int (*c_prim1)(int);
typedef int (*c_prim2)(int,int);
typedef int (*c_prim3)(int,int,int);
typedef int (*c_prim4)(int,int,int,int);
typedef int (*c_prim5)(int,int,int,int,int);
typedef int (*c_primN)(value*,int);

#define DynError(cond,err)		if( cond ) { *(char*)NULL = 0; val_throw(alloc_string(#err)); }
#define Error(cond,err)			DynError(cond,err)
#define Instr(x)	case x:
#define Next		break;

#define PopMacro(n) \
		tmp = n; \
		Error(sp + tmp > vm->spmax, UNDERFLOW); \
		while( tmp-- > 0 ) \
			*sp++ = NULL;

#define BeginCall() \
		vm->sp = sp; \
		vm->csp = csp;

#define EndCall() \
		sp = vm->sp; \
		csp = vm->csp

#define SetupBeforeCall(this_arg) \
		value old_this = vm->this; \
		value old_env = vm->env; \
		vm->this = this_arg; \
		vm->env = ((vfunction*)acc)->env; \
		BeginCall(); \

#define RestoreAfterCall() \
		if( acc == NULL ) acc = (int)val_null; \
		vm->env = old_env; \
		vm->this = old_this; \
		EndCall();

#define DoCall(this_arg) \
		if( acc & 1 ) { \
			acc = (int)val_null; \
			PopMacro(*pc++); \
		} else if( val_tag(acc) == VAL_FUNCTION && *pc == ((vfunction*)acc)->nargs ) { \
			if( csp + 3 >= sp ) { \
				DynError( !neko_stack_expand(sp,csp,vm) , OVERFLOW ); \
				sp = vm->sp; \
				csp = vm->csp; \
			} \
			*++csp = (int)(pc+1); \
			*++csp = (int)env; \
			*++csp = (int)vm->this; \
			pc = (int*)((vfunction*)acc)->addr; \
			vm->this = this_arg; \
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
				case 5: \
					acc = ((c_prim5)((vfunction*)acc)->addr)(sp[4],sp[3],sp[2],sp[1],sp[0]); \
					break; \
				} \
				RestoreAfterCall(); \
			} \
			else if( ((vfunction*)acc)->nargs == VAR_ARGS ) { \
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
		BeginCall(); \
		acc = (int)val_compare((value)*sp,(value)acc); \
		EndCall(); \
		*sp++ = NULL; \
		acc = (int)((acc test 0 && acc != invalid_comparison)?val_true:val_false); \
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

#define ObjectOp(obj,param,id) { \
		BeginCall(); \
		acc = (int)val_ocall1((value)obj,id,(value)param); \
		EndCall(); \
	}

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

void neko_setup_trap( neko_vm *vm, int where ) {
	vm->sp -= 5;
	if( vm->sp <= vm->csp )
		DynError( !neko_stack_expand(vm->sp,vm->csp,vm) , OVERFLOW );	
	vm->sp[0] = (int)alloc_int((int)(vm->csp - vm->spmin));
	vm->sp[1] = (int)vm->this;
	vm->sp[2] = (int)vm->env;
	vm->sp[3] = address_int(where);
	vm->sp[4] = (int)alloc_int((int)vm->trap);
	vm->trap = (int*)(vm->spmax - vm->sp);
}

void neko_process_trap( neko_vm *vm ) {
	// pop csp
	int *sp;
	if( vm->trap == 0 )
		return;

	vm->trap = vm->spmax - (int)vm->trap;
	sp = vm->spmin + val_int(vm->trap[0]);
	while( vm->csp > sp )
		*vm->csp-- = NULL;

	// restore state
	vm->this = (value)vm->trap[1];
	vm->env = (value)vm->trap[2];

	// pop sp
	sp = vm->trap + 5;
	vm->trap = (int*)val_int(vm->trap[4]);
	while( vm->sp < sp )
		*vm->sp++ = NULL;
}

value neko_interp( neko_vm *vm, register int acc, register int *pc, value env ) {
	register int *sp = vm->sp;
	register int *csp = vm->csp;
	int tmp;
	int *init_sp = (int*)(vm->spmax - sp);
	jmp_buf old;
	memcpy(&old,&vm->start,sizeof(jmp_buf));
	if( setjmp(vm->start) ) {
		acc = (int)vm->this;
		// if uncaught or outside init stack, reraise
		if( vm->trap == 0 || vm->trap <= init_sp ) {
			memcpy(&vm->start,&old,sizeof(jmp_buf));
			longjmp(vm->start,1);
		}

		vm->trap = vm->spmax - (int)vm->trap;
		if( vm->trap < vm->sp ) {
			// trap outside stack
			vm->trap = 0;
			Error( 1 , INVALID_TRAP )
		}

		// pop csp
		csp = vm->spmin + val_int(vm->trap[0]);
		while( vm->csp > csp )
			*vm->csp-- = NULL;
	
		// restore state
		vm->this = (value)vm->trap[1];
		env = (value)vm->trap[2];
		pc = int_address(vm->trap[3]);

		// pop sp
		sp = vm->trap + 5;
		vm->trap = (int*)val_int(vm->trap[4]);
		while( vm->sp < sp )
			*vm->sp++ = NULL;
	}
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
		acc = (int)vm->this;
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
		Error( sp == vm->spmax , UNDERFLOW );
		if( val_is_object(acc) )
			otable_replace(((vobject*)acc)->table,(field)*pc,(value)*sp);
		*sp++ = NULL;
		pc++;
		Next;
	Instr(SetArray)
		Error( sp >= vm->spmax - 1 , UNDERFLOW);
		if( val_is_int(*sp) && val_is_array(acc) ) {
			int k = val_int(*sp);
			if( k >= 0 && k < val_array_size(acc) )
				val_array_ptr(acc)[k] = (value)sp[1];
		}
		*sp++ = NULL;
		*sp++ = NULL;
		Next;
	Instr(SetThis)
		vm->this = (value)acc;
		Next;
	Instr(Push)
		--sp;
		if( sp <= csp ) {
			DynError( !neko_stack_expand(sp,csp,vm) , OVERFLOW );
			sp = vm->sp;
			csp = vm->csp;
		}
		*sp = acc;
		Next;
	Instr(Pop)
		PopMacro(*pc++)
		Next;
	Instr(Call)
		DoCall(vm->this);
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
	Instr(Trap)
		sp -= 5;
		if( sp <= csp ) {
			DynError( !neko_stack_expand(sp,csp,vm) , OVERFLOW );
			sp = vm->sp;
			csp = vm->csp;
		}
		sp[0] = (int)alloc_int((int)(csp - vm->spmin));
		sp[1] = (int)vm->this;
		sp[2] = (int)env;
		sp[3] = address_int(*pc);
		sp[4] = (int)alloc_int((int)vm->trap);
		vm->trap = (int*)(vm->spmax - sp);
		pc++;
		Next;
	Instr(EndTrap)
		Error( vm->spmax - (int)vm->trap != sp , END_TRAP );
		vm->trap = (int*)val_int(sp[4]);
		PopMacro(5);
		Next;
	Instr(Ret)
		Error( csp - 2 < vm->spmin , UNDERFLOW );
		PopMacro( *pc++ );
		vm->this = (value)*csp;
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
				acc = (int)alloc_module_function(*(void**)(((vfunction*)acc)+1),(int)((vfunction*)acc)->addr,((vfunction*)acc)->nargs);
				((vfunction*)acc)->env = (value)tmp;
			} else
				acc = (int)val_null;
		}
		Next;
	Instr(MakeArray)
		{
			int n = *pc++;
			acc = (int)alloc_array(n);
			Error( sp + n > vm->spmax , UNDERFLOW);
			while( n-- ) {
				val_array_ptr(acc)[n] = (value)*sp;
				*sp++ = NULL;
			}
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
			else if( (tmp = (val_tag(*sp)&7)) == VAL_STRING  )
				AppendString(*sp,"%d",val_int(acc),true)
			else if( tmp == VAL_OBJECT )
				ObjectOp(*sp,acc,id_add)
			else
				acc = (int)val_null;
		} else if( *sp & 1 ) {
			if( val_tag(acc) == VAL_FLOAT )
				acc = (int)alloc_float(val_int(*sp) + val_float(acc));
			else if( (tmp = (val_tag(acc)&7)) == VAL_STRING )
				AppendString(acc,"%d",val_int(*sp),false)
			else if( tmp == VAL_OBJECT )
				ObjectOp(acc,*sp,id_preadd)
			else
				acc = (int)val_null;
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT )
			acc = (int)alloc_float(val_float(*sp) + val_float(acc));
		else if( (tmp = (val_tag(acc)&7)) == VAL_STRING && (val_tag(*sp)&7) == VAL_STRING ) {
			int len1 = val_strlen(*sp);
			int len2 = val_strlen(acc);
			value v = alloc_empty_string(len1+len2);
			memcpy((char*)val_string(v),val_string(*sp),len1);
			memcpy((char*)val_string(v)+len1,val_string(acc),len2+1);
			acc = (int)v;
		} else if( tmp == VAL_STRING || (val_tag(*sp)&7) == VAL_STRING ) {
			buffer b = alloc_buffer(NULL);
			BeginCall();
			val_buffer(b,(value)*sp);
			val_buffer(b,(value)acc);
			EndCall();
			acc = (int)buffer_to_string(b);
		} else if( tmp == VAL_OBJECT )
			ObjectOp(acc,*sp,id_preadd)
		else if( (val_tag(*sp)&7) == VAL_OBJECT )
			ObjectOp(*sp,acc,id_add)
		else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(Sub)
		NumberOp(-,SUB)
	Instr(Mult)
		NumberOp(*,MULT)
	Instr(Div)
		if( val_is_number(acc) && val_is_number(*sp) )
			acc = (int)alloc_float( ((tfloat)val_number(*sp)) / val_number(acc) );
		else
			acc = (int)val_null;	
		*sp++ = NULL;
		Next;
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
		Error( sp == vm->spmax , UNDERFLOW );
		BeginCall();
		acc = (int)((val_compare((value)*sp,(value)acc) == 0)?val_false:val_true);
		EndCall();
		*sp++ = NULL;
		Next
	Instr(Lt)
		Test(<)
	Instr(Lte)
		Test(<=)
	Instr(Gt)
		Test(>)
	Instr(Gte)
		Test(>=)
	Instr(Last)
		goto end;
	}}
end:
	vm->sp = sp;
	vm->csp = csp;
	memcpy(&vm->start,&old,sizeof(jmp_buf));
	return (value)acc;
}

/* ************************************************************************ */
