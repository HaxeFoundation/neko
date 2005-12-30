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
#include "context.h"
#include "opcodes.h"
#include "vm.h"
#include "neko_mod.h"
#include "objtable.h"

#if defined(__GNUC__) && defined(__i386__)
#	define ACC_BACKUP	int_val __acc = acc;
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

#define ERASE 0

#define address_int(a)	(((int_val)(a)) | 1)
#define int_address(a)	(int_val*)(a & ~1)

extern field id_add, id_radd, id_sub, id_rsub, id_mult, id_rmult, id_div, id_rdiv, id_mod, id_rmod;
extern field id_get, id_set;
extern value alloc_module_function( void *m, int_val pos, int nargs );

value NEKO_TYPEOF[] = {
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
		int p = (int)fwrite(s,1,len,stdout);
		if( p <= 0 ) {
			fputs("[ABORTED]",stdout);
			break;
		}
		len -= p;
		s += p;
	}
	fflush(stdout);
}

EXTERN neko_vm *neko_vm_alloc( neko_params *p ) {
	neko_vm *vm = (neko_vm*)alloc(sizeof(neko_vm));
	vm->spmin = (int_val*)alloc(INIT_STACK_SIZE*sizeof(int_val));
	vm->print = (p && p->printer)?p->printer:default_printer;
	vm->custom = p?p->custom:NULL;
	vm->exc_stack = alloc_array(0);
	vm->spmax = vm->spmin + INIT_STACK_SIZE;
	vm->ncalls = 0;
	vm->sp = vm->spmax;
	vm->csp = vm->spmin - 1;
	vm->vthis = val_null;
	vm->env = alloc_array(0);
	vm->args = alloc_array(p?p->nargs:0);
	if( p ) {
		int i;
		for(i=0;i<p->nargs;i++)
			val_array_ptr(vm->args)[i] = alloc_string(p->args[i]);
	}
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

EXTERN value neko_vm_execute( neko_vm *vm, void *_m ) {
	unsigned int i;
	neko_module *m = (neko_module*)_m;
	value old_env = vm->env, ret;
	value old_this = vm->vthis;
	neko_vm_select(vm);
	for(i=0;i<m->nfields;i++)
		val_id(val_string(m->fields[i]));
	vm->env = alloc_array(0);
	vm->vthis = val_null;
	ret = neko_interp(vm,m,(int_val)val_null,m->code);
	vm->env = old_env;
	vm->vthis = old_this;
	return ret;
}

EXTERN value neko_exc_stack( neko_vm *vm ) {
	return vm->exc_stack;
}

static value neko_flush_stack( int_val *cspup, int_val *csp, value old );

EXTERN value neko_call_stack( neko_vm *vm ) {
	return neko_flush_stack(vm->csp,vm->spmin - 1,NULL);
}

typedef int_val (*c_prim0)();
typedef int_val (*c_prim1)(int_val);
typedef int_val (*c_prim2)(int_val,int_val);
typedef int_val (*c_prim3)(int_val,int_val,int_val);
typedef int_val (*c_prim4)(int_val,int_val,int_val,int_val);
typedef int_val (*c_prim5)(int_val,int_val,int_val,int_val,int_val);
typedef int_val (*c_primN)(value*,int);

#define RuntimeError(err)	{ pc++; PushInfos(); BeginCall(); val_throw(alloc_string(err)); }
#define CallFailure()		{ pc--; RuntimeError("Invalid call"); }

#define Instr(x)	case x:
#define Next		break;

#define PopMacro(n) { \
		int tmp = (int)n; \
		while( tmp-- > 0 ) \
			*sp++ = ERASE; \
	}

#define BeginCall() \
		vm->sp = sp; \
		vm->csp = csp;

#define EndCall() \
		sp = vm->sp; \
		csp = vm->csp

#define PushInfos() \
		if( csp + 4 >= sp ) { \
			STACK_EXPAND; \
			sp = vm->sp; \
			csp = vm->csp; \
		} \
		*++csp = (int_val)pc; \
		*++csp = (int_val)vm->env; \
		*++csp = (int_val)vm->vthis; \
		*++csp = (int_val)m;

#define PopInfos(restpc) \
		m = (neko_module*)*csp; \
		*csp-- = ERASE; \
		vm->vthis = (value)*csp; \
		*csp-- = ERASE; \
		vm->env = (value)*csp; \
		*csp-- = ERASE; \
		if( restpc ) pc = (int_val*)*csp; \
		*csp-- = ERASE;

#define SetupBeforeCall(this_arg) \
		vfunction *f = (vfunction*)acc; \
		PushInfos(); \
		vm->vthis = this_arg; \
		vm->env = ((vfunction*)acc)->env; \
		BeginCall();

#define RestoreAfterCall() \
		if( acc == (int_val)NULL ) val_throw( (value)f->module ); \
		EndCall(); \
		PopInfos(false);

#define DoCall(this_arg,pc_args) \
		if( acc & 1 ) \
			CallFailure() \
		else if( val_tag(acc) == VAL_FUNCTION && pc_args == ((vfunction*)acc)->nargs ) { \
			PushInfos(); \
			m = (neko_module*)((vfunction*)acc)->module; \
			pc = (int_val*)((vfunction*)acc)->addr; \
			vm->vthis = this_arg; \
			vm->env = ((vfunction*)acc)->env; \
		} else if( val_tag(acc) == VAL_PRIMITIVE ) { \
			if( pc_args == ((vfunction*)acc)->nargs ) { \
				SetupBeforeCall(this_arg); \
				switch( pc_args ) { \
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
				int_val args[CALL_MAX_ARGS]; \
				int_val tmp; \
				SetupBeforeCall(this_arg); \
				sp += pc_args; \
				for(tmp=0;tmp<pc_args;tmp++) \
					args[tmp] = *--sp; \
				acc = ((c_primN)((vfunction*)acc)->addr)((value*)args,(int)pc_args); \
				RestoreAfterCall(); \
			} else \
				CallFailure(); \
			PopMacro(pc_args); \
		} else \
			CallFailure();

#define IntOp(op) \
		if( (acc & 1) && (*sp & 1) ) \
			acc = (int_val)alloc_int(val_int(*sp) op val_int(acc)); \
		else \
			RuntimeError(#op); \
		*sp++ = ERASE; \
		Next

#define Test(test) \
		BeginCall(); \
		acc = (int_val)val_compare((value)*sp,(value)acc); \
		EndCall(); \
		*sp++ = ERASE; \
		acc = (int_val)((acc test 0 && acc != invalid_comparison)?val_true:val_false); \
		Next

#define SUB(x,y) ((x) - (y))
#define MULT(x,y) ((x) * (y))
#define DIV(x,y) ((x) / (y))

#define NumberOp(op,fop,id_op,id_rop) \
		if( (acc & 1) && (*sp & 1) ) \
			acc = (int_val)alloc_int(val_int(*sp) op val_int(acc)); \
		else if( acc & 1 ) { \
			if( val_tag(*sp) == VAL_FLOAT ) \
				acc = (int_val)alloc_float(fop(val_float(*sp),val_int(acc))); \
			else if( val_tag(*sp) == VAL_OBJECT ) \
			    ObjectOp(*sp,acc,id_op) \
			else \
				RuntimeError(#op); \
		} else if( *sp & 1 ) { \
			if( val_tag(acc) == VAL_FLOAT ) \
				acc = (int_val)alloc_float(fop(val_int(*sp),val_float(acc))); \
			else if( val_tag(acc) == VAL_OBJECT ) \
				ObjectOp(acc,*sp,id_rop) \
			else \
				RuntimeError(#op); \
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT ) \
			acc = (int_val)alloc_float(fop(val_float(*sp),val_float(acc))); \
		else if( val_tag(*sp) == VAL_OBJECT ) \
			ObjectOp(*sp,acc,id_op) \
		else if( val_tag(acc) == VAL_OBJECT ) \
			ObjectOp(acc,*sp,id_rop) \
		else \
			RuntimeError(#op); \
		*sp++ = ERASE; \
		Next;

#define ObjectOp(obj,param,id) { \
		BeginCall(); \
		acc = (int_val)val_ocall1((value)obj,id,(value)param); \
		EndCall(); \
	}

extern int neko_stack_expand( int_val *sp, int_val *csp, neko_vm *vm );
extern value append_int( neko_vm *vm, value str, int x, bool way );
extern value append_strings( value s1, value s2 );

#define STACK_EXPAND { \
		ACC_BACKUP; \
		if( !neko_stack_expand(sp,csp,vm) ) val_throw(alloc_string("Stack Overflow")); \
		ACC_RESTORE; \
}

static value neko_flush_stack( int_val *cspup, int_val *csp, value old ) {
	int ncalls = (int)((cspup - csp) / 4);
	value stack_trace = alloc_array(ncalls + ((old == NULL)?0:val_array_size(old)));
	value *st = val_array_ptr(stack_trace);
	neko_module *m;
	while( csp != cspup ) {
		m = (neko_module*)csp[4];
		if( m ) {
			if( !val_is_null(m->debuginf) ) {
				int ppc = (int)((((int_val**)csp)[1]-2) - m->code);
				*st = val_array_ptr(m->debuginf)[ppc];
			} else
				*st = m->name;
		} else
			*st = val_null;
		st++;
		if( old ) {
			*++csp = ERASE;
			*++csp = ERASE;
			*++csp = ERASE;
			*++csp = ERASE;
		} else
			csp += 4;
	}
	if( old ) {
		value *oldst = val_array_ptr(old);
		ncalls = val_array_size(old);
		while( ncalls-- )
			*st++ = *oldst++;
	}
	return stack_trace;
}

void neko_setup_trap( neko_vm *vm, int_val where ) {
	vm->sp -= 6;
	if( vm->sp <= vm->csp && !neko_stack_expand(vm->sp,vm->csp,vm) )
		val_throw(alloc_string("Stack Overflow"));
	vm->sp[0] = (int_val)alloc_int((int_val)(vm->csp - vm->spmin));
	vm->sp[1] = (int_val)vm->vthis;
	vm->sp[2] = (int_val)vm->env;
	vm->sp[3] = address_int(where);
	vm->sp[4] = (int_val)val_null;
	vm->sp[5] = (int_val)alloc_int((int_val)vm->trap);
	vm->trap = vm->spmax - vm->sp;
}

void neko_process_trap( neko_vm *vm ) {
	// pop csp
	int_val *sp;
	int_val *trap;	
	if( vm->trap == 0 )
		return;

	trap = vm->spmax - vm->trap;
	sp = vm->spmin + val_int(trap[0]);
	vm->exc_stack = neko_flush_stack(vm->csp,sp,vm->exc_stack);

	vm->csp = sp;

	// restore state
	vm->vthis = (value)trap[1];
	vm->env = (value)trap[2];

	// pop sp
	sp = trap + 6;
	vm->trap = val_int(trap[5]);
	while( vm->sp < sp )
		*vm->sp++ = ERASE;
}

static int_val interp_loop( neko_vm *vm, neko_module *m, int_val _acc, int_val *_pc ) {
	register int_val acc ACC_REG = _acc;
	register int_val *pc PC_REG = _pc;
	register int_val *sp SP_REG = vm->sp;
	int_val *csp = vm->csp;
	while( true ) {
#ifdef NEKO_PROF
		if( *pc != Last ) pc[PROF_SIZE]++;
#endif
		switch( *pc++ ) {
	Instr(AccNull)
		acc = (int_val)val_null;
		Next;
	Instr(AccTrue)
		acc = (int_val)val_true;
		Next;
	Instr(AccFalse)
		acc = (int_val)val_false;
		Next;
	Instr(AccThis)
		acc = (int_val)vm->vthis;
		Next;
	Instr(AccInt)
		acc = *pc++;
		Next;
	Instr(AccStack0)
		acc = *sp;
		Next;
	Instr(AccStack1)
		acc = sp[1];
		Next;
	Instr(AccStack)
		acc = sp[*pc++];
		Next;
	Instr(AccGlobal)
		acc = *(int_val*)(*pc++);
		Next;
	Instr(AccEnv)
		if( *pc >= val_array_size(vm->env) ) RuntimeError("Reading Outside Env");
		acc = (int_val)val_array_ptr(vm->env)[*pc++];
		Next;
	Instr(AccField)
		if( val_is_object(acc) ) {
			value *f = otable_find(((vobject*)acc)->table,(field)*pc);
			acc = (int_val)(f?*f:val_null);
		} else
			RuntimeError("Invalid field access");
		pc++;
		Next;
	Instr(AccArray)
		if( val_is_int(acc) && val_is_array(*sp) ) {
			int k = val_int(acc);
			if( k < 0 || k >= val_array_size(*sp) )
				acc = (int_val)val_null;
			else
				acc = (int_val)val_array_ptr(*sp)[k];
		} else if( val_is_object(*sp) )
			ObjectOp(*sp,acc,id_get)
		else 
			RuntimeError("Invalid array access");
		*sp++ = ERASE;
		Next;
	Instr(AccIndex0)
		if( val_is_array(acc) ) {
			if( val_array_size(acc) )
				acc = (int_val)*val_array_ptr(acc);
			else
				acc = (int_val)val_null;
		} else if( val_is_object(acc) )
			ObjectOp(acc,alloc_int(0),id_get)
		else
			RuntimeError("Invalid array access");
		Next;
	Instr(AccIndex1)
		if( val_is_array(acc) ) {
			if( val_array_size(acc) > 1 )
				acc = (int_val)val_array_ptr(acc)[1];
			else
				acc = (int_val)val_null;
		} else if( val_is_object(acc) )
			ObjectOp(acc,alloc_int(1),id_get)
		else
			RuntimeError("Invalid array access");
		Next;
	Instr(AccIndex)
		if( val_is_array(acc) ) {
			if( *pc < 0 || *pc >= val_array_size(acc) )
				acc = (int_val)val_null;
			else
				acc = (int_val)val_array_ptr(acc)[*pc];
		} else if( val_is_object(acc) )
			ObjectOp(acc,alloc_int(*pc),id_get)
		else
			RuntimeError("Invalid array access");
		*pc++;
		Next;
	Instr(AccBuiltin)
		acc = *pc++;
		Next;
	Instr(SetStack)
		sp[*pc++] = acc;
		Next;
	Instr(SetGlobal)
		*(int_val*)(*pc++) = acc;
		Next;
	Instr(SetEnv)
		if( *pc >= val_array_size(vm->env) ) RuntimeError("Writing Outside Env");
		val_array_ptr(vm->env)[*pc++] = (value)acc;
		Next;
	Instr(SetField)
		if( val_is_object(*sp) ) {
			ACC_BACKUP;
			otable_replace(((vobject*)*sp)->table,(field)*pc,(value)acc);
			ACC_RESTORE;
		} else
			RuntimeError("Invalid field access");
		*sp++ = ERASE;
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
		*sp++ = ERASE;
		*sp++ = ERASE;
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
		*sp++ = ERASE;
		Next;
	Instr(SetThis)
		vm->vthis = (value)acc;
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
	Instr(Apply)
		if( !val_is_function(acc) )
			RuntimeError("$apply");
		{
			int fargs = val_fun_nargs(acc);
			if( fargs == *pc || fargs == VAR_ARGS )
				goto do_call;
			if( *pc > fargs )
				RuntimeError("$apply");
			{
				int i = fargs;
				ACC_BACKUP
				value env = alloc_array(fargs + 1);
				ACC_RESTORE;
				val_array_ptr(env)[0] = (value)acc;
				while( i > *pc )
					val_array_ptr(env)[i--] = val_null;
				while( i ) {
					val_array_ptr(env)[i--] = (value)*sp;
					*sp++ = ERASE;
				}
				acc = (int_val)alloc_apply((int)(fargs - *pc++),env);
			}
		}
		Next;
	Instr(TailCall)
		{
			int stack = (int)((*pc) >> 3);
			int nargs = (int)((*pc) & 7);
			int i = nargs;
			stack -= nargs;
			sp += nargs;
			while( i > 0 ) {
				sp--;
				sp[stack] = *sp;
				i--;
			}
			while( stack-- > 0 )
				*sp++ = ERASE;			
			PopInfos(true);
			DoCall(vm->vthis,nargs);
		}
		Next;
	Instr(Call)
		do_call:
		pc++;
		DoCall(vm->vthis,pc[-1]);
		Next;
	Instr(ObjCall)
		{
			value vtmp = (value)*sp;
			*sp++ = ERASE;
			pc++;
			DoCall(vtmp,pc[-1]);
		}
		Next;
	Instr(Jump)
		pc = (int_val*)*pc;
		Next;
	Instr(JumpIf)
		if( acc == (int_val)val_true )
			pc = (int_val*)*pc;
		else
			pc++;
		Next;
	Instr(JumpIfNot)
		if( acc != (int_val)val_true )
			pc = (int_val*)*pc;
		else
			pc++;
		Next;
	Instr(Trap)
		sp -= 6;
		if( sp <= csp ) {
			STACK_EXPAND;
			sp = vm->sp;
			csp = vm->csp;
		}
		sp[0] = (int_val)alloc_int((int_val)(csp - vm->spmin));
		sp[1] = (int_val)vm->vthis;
		sp[2] = (int_val)vm->env;
		sp[3] = address_int(*pc);
		sp[4] = address_int(m);
		sp[5] = (int_val)alloc_int(vm->trap);
		vm->trap = vm->spmax - sp;
		pc++;
		Next;
	Instr(EndTrap)
		if( vm->spmax - vm->trap != sp ) RuntimeError("Invalid End Trap");
		vm->trap = val_int(sp[5]);
		PopMacro(6);
		Next;
	Instr(Ret)
		PopMacro( *pc++ );
		PopInfos(true);
		Next;
	Instr(MakeEnv)
		{
			int n = (int)(*pc++);
			ACC_BACKUP
			int_val tmp = (int_val)alloc_array(n);
			ACC_RESTORE;
			while( n-- ) {
				val_array_ptr(tmp)[n] = (value)*sp;
				*sp++ = ERASE;
			}
			if( val_is_int(acc) || val_tag(acc) != VAL_FUNCTION ) {
				pc--;
				RuntimeError("Invalid environment");
			}
			acc = (int_val)alloc_module_function(((vfunction*)acc)->module,(int_val)((vfunction*)acc)->addr,((vfunction*)acc)->nargs);
			((vfunction*)acc)->env = (value)tmp;
		}
		Next;
	Instr(MakeArray)
		{
			int n = (int)*pc++;
			value arr = alloc_array(n+1);
			while( n ) {
				val_array_ptr(arr)[n] = (value)*sp;
				*sp++ = ERASE;
				n--;
			}
			val_array_ptr(arr)[0] = (value)acc;
			acc = (int_val)arr;
		}
		Next;
	Instr(Bool)
		acc = (acc == (int_val)val_false || acc == (int_val)val_null || acc == 1)?(int_val)val_false:(int_val)val_true;
		Next;
	Instr(Not)
		acc = (acc == (int_val)val_false || acc == (int_val)val_null || acc == 1)?(int_val)val_true:(int_val)val_false;
		Next;
	Instr(IsNull)
		acc = (int_val)((acc == (int_val)val_null)?val_true:val_false);
		Next;
	Instr(IsNotNull)
		acc = (int_val)((acc == (int_val)val_null)?val_false:val_true);
		Next;
	Instr(Add)
		if( (acc & 1) && (*sp & 1) )
			acc = (int_val)alloc_int(val_int(*sp) + val_int(acc));
		else if( acc & 1 ) {
			if( val_tag(*sp) == VAL_FLOAT )
				acc = (int_val)alloc_float(val_float(*sp) + val_int(acc));
			else if( (val_tag(*sp)&7) == VAL_STRING  )
				acc = (int_val)append_int(vm,(value)*sp,val_int(acc),true);
			else if( val_tag(*sp) == VAL_OBJECT )
				ObjectOp(*sp,acc,id_add)
			else
				RuntimeError("+");
		} else if( *sp & 1 ) {
			if( val_tag(acc) == VAL_FLOAT )
				acc = (int_val)alloc_float(val_int(*sp) + val_float(acc));
			else if( (val_tag(acc)&7) == VAL_STRING )
				acc = (int_val)append_int(vm,(value)acc,val_int(*sp),false);
			else if( val_tag(acc) == VAL_OBJECT )
				ObjectOp(acc,*sp,id_radd)
			else
				RuntimeError("+");
		} else if( val_tag(acc) == VAL_FLOAT && val_tag(*sp) == VAL_FLOAT )
			acc = (int_val)alloc_float(val_float(*sp) + val_float(acc));
		else if( (val_tag(acc)&7) == VAL_STRING && (val_tag(*sp)&7) == VAL_STRING )
			acc = (int_val)append_strings((value)*sp,(value)acc);
		else if( (val_tag(acc)&7) == VAL_STRING || (val_tag(*sp)&7) == VAL_STRING ) {
			ACC_BACKUP
			buffer b = alloc_buffer(NULL);
			BeginCall();
			val_buffer(b,(value)*sp);
			ACC_RESTORE;
			val_buffer(b,(value)acc);
			EndCall();
			acc = (int_val)buffer_to_string(b);
		} else if( val_tag(*sp) == VAL_OBJECT )
			ObjectOp(*sp,acc,id_add)
		else if( val_tag(acc) == VAL_OBJECT )
			ObjectOp(acc,*sp,id_radd)
		else
			RuntimeError("+");
		*sp++ = ERASE;
		Next;
	Instr(Sub)
		NumberOp(-,SUB,id_sub,id_rsub)
	Instr(Mult)
		NumberOp(*,MULT,id_mult,id_rmult)
	Instr(Div)
		if( val_is_number(acc) && val_is_number(*sp) )
			acc = (int_val)alloc_float( ((tfloat)val_number(*sp)) / val_number(acc) );
		else if( val_is_object(acc) )
			ObjectOp(acc,*sp,id_rdiv)
		else if( val_is_object(*sp) )
			ObjectOp(*sp,acc,id_div)
		else
			RuntimeError("/");
		*sp++ = ERASE;
		Next;
	Instr(Mod)
		if( acc == 1 ) {
			acc	= (int_val)val_null;
			*sp++ = ERASE;
			Next;
		}
		NumberOp(%,fmod,id_mod,id_rmod);
	Instr(Shl)
		IntOp(<<);
	Instr(Shr)
		IntOp(>>);
	Instr(UShr)
		if( (acc & 1) && (*sp & 1) )
			acc = (int_val)alloc_int(((unsigned int)val_int(*sp)) >> val_int(acc));
		else
			RuntimeError(">>>");
		*sp++ = ERASE;
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
		acc = (int_val)((val_compare((value)*sp,(value)acc) == 0)?val_false:val_true);
		EndCall();
		*sp++ = ERASE;
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
		acc = (int_val)(val_is_int(acc) ? alloc_int(1) : NEKO_TYPEOF[val_tag(acc)&7]);
		Next;
	Instr(Compare)
		BeginCall();
		acc = (int_val)val_compare((value)*sp,(value)acc);
		EndCall();
		acc = (int_val)((acc == invalid_comparison)?val_null:alloc_int(acc));
		*sp++ = ERASE;
		Next;
	Instr(PhysCompare)
		acc = (int_val)(( *sp > acc )?alloc_int(1):(( *sp < acc )?alloc_int(-1):alloc_int(0)));
		*sp++ = ERASE;
		Next;
	Instr(Hash)
		if( val_is_string(acc) ) {
			BeginCall();
			acc = (int_val)alloc_int( val_id(val_string(acc)) );
		} else
			RuntimeError("$hash");
		Next;
	Instr(New)
		acc = (int_val)alloc_object((value)acc);
		Next;
	Instr(JumpTable)
		if( val_is_int(acc) && ((unsigned)acc) < ((unsigned)*pc) )
			pc += acc;
		else
			pc += *pc + 1;
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

value neko_interp( neko_vm *vm, void *m, int_val acc, int_val *pc ) {
	int_val *sp, *csp, *trap;
	int_val init_sp = vm->spmax - vm->sp;
	int old_ncalls = vm->ncalls;
	jmp_buf old;
	memcpy(&old,&vm->start,sizeof(jmp_buf));
	if( setjmp(vm->start) ) {
		vm->ncalls = old_ncalls;
		acc = (int_val)vm->vthis;

		// if uncaught or outside init stack, reraise
		if( vm->trap == 0 || vm->trap <= init_sp ) {
			memcpy(&vm->start,&old,sizeof(jmp_buf));
			longjmp(vm->start,1);
		}

		trap = vm->spmax - vm->trap;
		if( trap < vm->sp ) {
			// trap outside stack
			vm->trap = 0;
			val_throw(alloc_string("Invalid Trap"));
		}

		// pop csp
		csp = vm->spmin + val_int(trap[0]);
		vm->exc_stack = neko_flush_stack(vm->csp,csp,vm->exc_stack);
		vm->csp = csp;

		// restore state
		vm->vthis = (value)trap[1];
		vm->env = (value)trap[2];
		pc = int_address(trap[3]);
		m = (void*)int_address(trap[4]);

		// pop sp
		sp = trap + 6;
		vm->trap = val_int(trap[5]);
		while( vm->sp < sp )
			*vm->sp++ = ERASE;
	}
	acc = interp_loop(vm,(neko_module*)m,acc,pc);
	memcpy(&vm->start,&old,sizeof(jmp_buf));
	return (value)acc;
}

/* ************************************************************************ */
