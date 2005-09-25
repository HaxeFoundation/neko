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
#include <math.h>
#include <string.h>
#include <stdio.h>
#undef NULL
#include "context.h"
#include "load.h"
#include "interp.h"
#include "opcodes.h"
#include "vmcontext.h"
#include "objtable.h"

#if defined(__GNUC__) && defined(__i386__)
#	define ACC_SAVE
#	define ACC_BACKUP	int __acc = acc;
#	define ACC_RESTORE	acc = __acc;
#	define ACC_REG asm("%eax")
#	define PC_REG asm("%esi")
#	define SP_REG asm("%edi")
#else
#	define ACC_BACKUP
#	define ACC_RESTORE
#	define ACC_REG
#	define PC_REG
#	define SP_REG
#endif

#define address_int(a)	(((int)(a)) | 1)
#define int_address(a)	(int*)(a & ~1)

extern field id_add, id_radd, id_sub, id_rsub, id_mult, id_rmult, id_div, id_rdiv, id_mod, id_rmod;
extern field id_get, id_set;
extern value alloc_module_function( void *m, int pos, int nargs );

static value TYPEOF[] = {
	alloc_int(0),
	alloc_int(2),
	alloc_int(3),
	alloc_int(4),
	alloc_int(5),
	alloc_int(6),
	alloc_int(7),
	alloc_int(8)
};

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
}

EXTERN neko_vm *neko_vm_alloc( neko_params *p ) {
	neko_vm *vm = (neko_vm*)alloc(sizeof(neko_vm));	
	vm->spmin = (int*)alloc(INIT_STACK_SIZE*sizeof(int));
	vm->print = (p && p->printer)?p->printer:default_printer;
	vm->custom = p?p->custom:NULL;
	vm->spmax = vm->spmin + INIT_STACK_SIZE;
	vm->sp = vm->spmax;
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

typedef int (*c_prim0)();
typedef int (*c_prim1)(int);
typedef int (*c_prim2)(int,int);
typedef int (*c_prim3)(int,int,int);
typedef int (*c_prim4)(int,int,int,int);
typedef int (*c_prim5)(int,int,int,int,int);
typedef int (*c_primN)(value*,int);

#define Error(cond,err)		if( cond ) { failure(err); }
#define Instr(x)	case x:
#define Next		break;

#define PopMacro(n) { \
		int tmp = n; \
		while( tmp-- > 0 ) \
			*sp++ = NULL; \
	}

#define BeginCall() \
		vm->sp = sp; \
		vm->csp = csp;

#define EndCall() \
		sp = vm->sp; \
		csp = vm->csp

#define SetupBeforeCall(this_arg) \
		value old_this = vm->this; \
		value old_env = vm->env; \
		vfunction *f = (vfunction*)acc; \
		vm->this = this_arg; \
		vm->env = ((vfunction*)acc)->env; \
		BeginCall(); \

#define RestoreAfterCall() \
		if( acc == NULL ) val_throw( (value)f->module ); \
		vm->env = old_env; \
		vm->this = old_this; \
		EndCall();

#define DoCall(this_arg) \
		if( acc & 1 ) { \
			acc = (int)val_null; \
			PopMacro(*pc++); \
		} else if( val_tag(acc) == VAL_FUNCTION && *pc == ((vfunction*)acc)->nargs ) { \
			if( csp + 3 >= sp ) { \
				STACK_EXPAND; \
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
				int tmp; \
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
		if( (acc & 1) && (*sp & 1) ) \
			acc = (int)alloc_int(val_int(*sp) op val_int(acc)); \
		else \
			acc = (int)val_null; \
		*sp++ = NULL; \
		Next

#define Test(test) \
		BeginCall(); \
		acc = (int)val_compare((value)*sp,(value)acc); \
		EndCall(); \
		*sp++ = NULL; \
		acc = (int)((acc test 0 && acc != invalid_comparison)?val_true:val_false); \
		Next

#define SUB(x,y) ((x) - (y))
#define MULT(x,y) ((x) * (y))
#define DIV(x,y) ((x) / (y))

#define NumberOp(op,fop,id_op,id_rop) \
		if( (acc & 1) && (*sp & 1) ) \
			acc = (int)alloc_int(val_int(*sp) op val_int(acc)); \
		else if( acc & 1 ) { \
			if( val_tag(*sp) == VAL_FLOAT ) \
				acc = (int)alloc_float(fop(val_float(*sp),val_int(acc))); \
			else if( val_tag(*sp) == VAL_OBJECT ) \
			    ObjectOp(*sp,acc,id_op) \
			else \
				acc = (int)val_null; \
		} else if( *sp & 1 ) { \
			if( val_tag(acc) == VAL_FLOAT ) \
				acc = (int)alloc_float(fop(val_int(*sp),val_float(acc))); \
			else if( val_tag(acc) == VAL_OBJECT ) \
				ObjectOp(acc,*sp,id_rop) \
			else \
				acc = (int)val_null; \
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT ) \
			acc = (int)alloc_float(fop(val_float(*sp),val_float(acc))); \
		else if( val_tag(*sp) == VAL_OBJECT ) \
			ObjectOp(*sp,acc,id_op) \
		else if( val_tag(acc) == VAL_OBJECT ) \
			ObjectOp(acc,*sp,id_rop) \
		else \
			acc = (int)val_null; \
		*sp++ = NULL; \
		Next;

#define ObjectOp(obj,param,id) { \
		BeginCall(); \
		acc = (int)val_ocall1((value)obj,id,(value)param); \
		EndCall(); \
	}

extern int neko_stack_expand( int *sp, int *csp, neko_vm *vm );
extern value append_int( neko_vm *vm, value str, int x, bool way );
extern value append_strings( value s1, value s2 );

#undef OVERFLOW
#define OVERFLOW	"Stack Overflow"

#ifdef ACC_SAVE
#	define STACK_EXPAND { \
		int _acc = acc; \
		Error( !neko_stack_expand(sp,csp,vm) , OVERFLOW ); \
		acc = _acc; \
	}
#else
#	define STACK_EXPAND Error( !neko_stack_expand(sp,csp,vm) , OVERFLOW );
#endif

void neko_setup_trap( neko_vm *vm, int where ) {
	int acc = 0;
	int *sp, *csp;
	vm->sp -= 5;
	csp = vm->csp;
	sp = vm->sp;
	if( vm->sp <= vm->csp )
		STACK_EXPAND;
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

static int interp_loop( neko_vm *vm, int _acc, int *_pc, value env ) {
	register int acc ACC_REG = _acc;
	register int *pc PC_REG = _pc;
	register int *sp SP_REG = vm->sp;
	int *csp = vm->csp;
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
	Instr(AccStack)
		acc = sp[*pc++];
		Next;
	Instr(AccGlobal)
		acc = *(int*)(*pc++);
		Next;
	Instr(AccEnv)
		Error( *pc >= val_array_size(env) , "Reading Outside Env" );
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
		if( val_is_int(acc) && val_is_array(*sp) ) {
			int k = val_int(acc);
			if( k < 0 || k >= val_array_size(*sp) )
				acc = (int)val_null;
			else
				acc = (int)val_array_ptr(*sp)[k];
		} else if( val_is_object(*sp) )
			ObjectOp(*sp,acc,id_get)
		else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(AccIndex)
		if( val_is_array(acc) ) {
			if( *pc < 0 || *pc >= val_array_size(acc) )
				acc = (int)val_null;
			else
				acc = (int)val_array_ptr(acc)[*pc];
		} else if( val_is_object(acc) )
			ObjectOp(acc,alloc_int(*pc),id_get)
		else
			acc = (int)val_null;
		*pc++;
		Next;
	Instr(AccBuiltin)
		acc = *pc++;
		Next;
	Instr(SetStack)
		sp[*pc++] = acc;
		Next;
	Instr(SetGlobal)
		*(int*)(*pc++) = acc;
		Next;
	Instr(SetEnv)
		Error( *pc >= val_array_size(env) , "Writing Outside Env" );
		val_array_ptr(env)[*pc++] = (value)acc;
		Next;
	Instr(SetField)
		if( val_is_object(*sp) ) {
			ACC_BACKUP;
			otable_replace(((vobject*)*sp)->table,(field)*pc,(value)acc);
			ACC_RESTORE;
		}
		*sp++ = NULL;
		pc++;
		Next;
	Instr(SetArray)
		if( val_is_array(*sp) && val_is_int(sp[1]) ) {
			int k = val_int(sp[1]);
			if( k >= 0 && k < val_array_size(*sp) )
				val_array_ptr(*sp)[k] = (value)acc;
		} else if( val_is_object(*sp) ) {
			ACC_BACKUP;
			BeginCall();
			val_ocall2((value)*sp,id_set,(value)sp[1],(value)acc);
			EndCall();
			ACC_RESTORE;
		}
		*sp++ = NULL;
		*sp++ = NULL;
		Next;
	Instr(SetIndex)
		if( val_is_array(*sp) ) {
			if( *pc >= 0 && *pc < val_array_size(*sp) )
				val_array_ptr(*sp)[*pc] = (value)acc;
		} else if( val_is_object(*sp) ) {
			ACC_BACKUP;
			BeginCall();
			val_ocall2((value)*sp,id_set,(value)alloc_int(*pc),(value)acc);
			EndCall();
			ACC_RESTORE;
		}
		pc++;
		*sp++ = NULL;
		Next;
	Instr(SetThis)
		vm->this = (value)acc;
		Next;
	Instr(Push)
		--sp;
		if( sp <= csp ) {
			STACK_EXPAND;
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
		{
			value vtmp = (value)*sp; 
			*sp++ = NULL;
			DoCall(vtmp);
		}
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
			STACK_EXPAND;
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
		Error( vm->spmax - (int)vm->trap != sp , "Invalid End Trap" );
		vm->trap = (int*)val_int(sp[4]);
		PopMacro(5);
		Next;
	Instr(Ret)
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
			ACC_BACKUP
			int tmp = (int)alloc_array(n);
			ACC_RESTORE;
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
			while( n-- ) {
				val_array_ptr(acc)[n] = (value)*sp;
				*sp++ = NULL;
			}
		}
		Next;
	Instr(Bool)
		acc = (acc == (int)val_false || acc == (int)val_null || acc == 1)?(int)val_false:(int)val_true;
		Next;
	Instr(Not)
		acc = (acc == (int)val_false || acc == (int)val_null || acc == 1)?(int)val_true:(int)val_false;
		Next;
	Instr(IsNull)
		acc = (int)((acc == (int)val_null)?val_true:val_false);
		Next;
	Instr(IsNotNull)
		acc = (int)((acc == (int)val_null)?val_false:val_true);
		Next;
	Instr(Add)
		if( (acc & 1) && (*sp & 1) )
			acc = (int)alloc_int(val_int(*sp) + val_int(acc));
		else if( acc & 1 ) {
			if( val_tag(*sp) == VAL_FLOAT )
				acc = (int)alloc_float(val_float(*sp) + val_int(acc));
			else if( (val_tag(*sp)&7) == VAL_STRING  )
				acc = (int)append_int(vm,(value)*sp,val_int(acc),true);
			else if( val_tag(*sp) == VAL_OBJECT )
				ObjectOp(*sp,acc,id_add)
			else
				acc = (int)val_null;
		} else if( *sp & 1 ) {
			if( val_tag(acc) == VAL_FLOAT )
				acc = (int)alloc_float(val_int(*sp) + val_float(acc));
			else if( (val_tag(acc)&7) == VAL_STRING )
				acc = (int)append_int(vm,(value)acc,val_int(*sp),false);
			else if( val_tag(acc) == VAL_OBJECT )
				ObjectOp(acc,*sp,id_radd)
			else
				acc = (int)val_null;
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT )
			acc = (int)alloc_float(val_float(*sp) + val_float(acc));
		else if( (val_tag(acc)&7) == VAL_STRING && (val_tag(*sp)&7) == VAL_STRING )
			acc = (int)append_strings((value)*sp,(value)acc);
		else if( (val_tag(acc)&7) == VAL_STRING || (val_tag(*sp)&7) == VAL_STRING ) {
			ACC_BACKUP
			buffer b = alloc_buffer(NULL);
			BeginCall();
			val_buffer(b,(value)*sp);
			ACC_RESTORE;
			val_buffer(b,(value)acc);
			EndCall();
			acc = (int)buffer_to_string(b);
		} else if( val_tag(*sp) == VAL_OBJECT )
			ObjectOp(*sp,acc,id_add)
		else if( val_tag(acc) == VAL_OBJECT )
			ObjectOp(acc,*sp,id_radd)
		else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(Sub)
		NumberOp(-,SUB,id_sub,id_rsub)
	Instr(Mult)
		NumberOp(*,MULT,id_mult,id_rmult)
	Instr(Div)
		if( val_is_number(acc) && val_is_number(*sp) )
			acc = (int)alloc_float( ((tfloat)val_number(*sp)) / val_number(acc) );
		else if( val_is_object(acc) )
			ObjectOp(acc,*sp,id_rdiv)
		else if( val_is_object(*sp) )
			ObjectOp(*sp,acc,id_div)
		else
			acc = (int)val_null;
		*sp++ = NULL;
		Next;
	Instr(Mod)
		if( acc == 1 ) {
			acc	= (int)val_null;
			*sp++ = NULL;
			Next;
		}
		NumberOp(%,fmod,id_mod,id_rmod);
	Instr(Shl)
		IntOp(<<);
	Instr(Shr)
		IntOp(>>);
	Instr(UShr)
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
		BeginCall();
		acc = (int)((val_compare((value)*sp,(value)acc) == 0)?val_false:val_true);
		EndCall();
		*sp++ = NULL;
		Next;
	Instr(Lt)
		Test(<)
	Instr(Lte)
		Test(<=)
	Instr(Gt)
		Test(>)
	Instr(Gte)
		Test(>=)
	Instr(TypeOf)
		acc = (int)(val_is_int(acc) ? alloc_int(1) : TYPEOF[val_tag(acc)&7]);
		Next;
	Instr(Compare)
		BeginCall();
		acc = (int)val_compare((value)*sp,(value)acc);
		EndCall();
		acc = (int)((acc == invalid_comparison)?val_null:alloc_int(acc));
		*sp++ = NULL;
		Next;
	Instr(Hash)
		if( val_is_string(acc) )
			acc = (int)alloc_int( (int)val_id(val_string(acc)));
		else
			acc = (int)val_null;
		Next;
	Instr(New)
		acc = (int)alloc_object((value)acc);
		Next;
	Instr(Last)
		goto end;
#ifdef _MSC_VER
	default:
         __assume(0);
#endif
	}}
end:
	vm->sp = sp;
	vm->csp = csp;
	return acc;
}

value neko_interp( neko_vm *vm, int acc, int *pc, value env ) {
	int *sp, *csp;
	int *init_sp = (int*)(vm->spmax - vm->sp);
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
			Error( 1 , "Invalid Trap" )
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
	acc = interp_loop(vm,acc,pc,env);
	memcpy(&vm->start,&old,sizeof(jmp_buf));
	return (value)acc;
}

/* ************************************************************************ */
