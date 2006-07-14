/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Motion-Twin											*/
/*																			*/
/* This library is free software; you can redistribute it and/or			*/
/* modify it under the terms of the GNU Lesser General Public				*/
/* License as published by the Free Software Foundation; either				*/
/* version 2.1 of the License, or (at your option) any later version.		*/
/*																			*/
/* This library is distributed in the hope that it will be useful,			*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU		*/
/* Lesser General Public License or the LICENSE file for more details.		*/
/*																			*/
/* ************************************************************************ */
#include "vm.h"
#include "neko_mod.h"
#define PARAMETER_TABLE
#include "opcodes.h"
#include <string.h>

#if defined(NEKO_X86) && defined(_WIN32) && defined(_DEBUG)
#define JIT_ENABLE
#endif

#ifdef JIT_ENABLE

extern int neko_stack_expand( int_val *sp, int_val *csp, neko_vm *vm );

typedef union {
	void *p;
	unsigned char *b;
	unsigned int *w;
} jit_buffer;

typedef struct _jlist {
	void *j;	
	int target;
	struct _jlist *next;
} jlist;

typedef struct {
	jit_buffer buf;
	void *baseptr;
	neko_module *module;
	int curpc;
	int size;
	int *pos;
	jlist *jumps;
} jit_ctx;

enum Special {
	VThis,
	VEnv,
	VModule,
	VVm,
	VSpMax,
	VTrap,
};

enum Callback {
	CALLBACK,
	PC_CUR,
	PC_ARG
};

enum CallMode {
	NORMAL,
	THIS_CALL,
	TAIL_CALL,
};

#define Eax 0
#define Ebx 3
#define Ecx 1
#define Edx 2
#define Esp 4
#define Ebp 5
#define Esi 6
#define Edi 7

#define ACC		Eax
#define SP		Edi
#define CSP		Esi
#define VM		Ebx
#define TMP		Ecx
#define TMP2	Edx

#define B(bv)	*buf.b++ = bv
#define W(wv)	*buf.w++ = wv

#define Mult1	0
#define Mult2	1
#define Mult4	2
#define Mult8	3

#define JAlways		0
#define JLt			0x82
#define JGte		0x83
#define JEq			0x84
#define JNeq		0x85
#define JLte		0x86
#define JGt			0x87
#define JSignLt		0x8C
#define JSignGte	0x8D
#define JSignLte	0x8E
#define JSignGt		0x8F

#define ERROR	failure("JIT error")
#define CONST(v)		((int)(int_val)v)
#define PATCH_JUMP(local)		*(int*)local = (int)((int_val)buf.p - ((int_val)local + 4))
#define FIELD(n)				((int)((n) * 4))
#define POS()					((int)((int_val)ctx->buf.p - (int_val)ctx->baseptr))
#define GET_PC()				CONST(ctx->module->code + ctx->curpc)

#define INIT_BUFFER				register jit_buffer buf = ctx->buf
#define END_BUFFER				ctx->buf = buf

#define MOD_RM(mod,reg,rm)		B((mod << 6) | (reg << 3) | rm)
#define SIB						MOD_RM
#define IS_SBYTE(c)				( (c) >= -128 && (c) < 128 )

#define OP_RM(op,mod,reg,rm)	{ B(op); MOD_RM(mod,reg,rm); }
#define OP_ADDR(op,addr,reg,rm) { B(op); \
								MOD_RM((addr == 0 && reg != Ebp)?0:(IS_SBYTE(addr)?1:2),rm,reg); \
								if( reg == Esp ) B(0x24); \
								if( addr == 0 && reg != Ebp ) {} \
								else if IS_SBYTE(addr) B(addr); \
								else W(addr); }

// OPCODES :
// _r : register
// _c : constant
// _b : 8-bit constant
// _a : [constant]
// _p : [reg + constant:idx]
// _x : [reg + reg:idx * mult]

#define XRet()					B(0xC3)
#define XMov_rr(dst,src)		OP_RM(0x8B,3,dst,src)
#define XMov_rc(dst,cst)		B(0xB8+dst); W(cst)
#define XMov_rp(dst,reg,idx)	OP_ADDR(0x8B,idx,dst,reg)
#define XMov_ra(dst,addr)		OP_RM(0x8B,0,dst,5); W(addr)
#define XMov_rx(dst,r,idx,mult) OP_RM(0x8B,0,dst,4); SIB(Mult##mult,idx,r)
#define XMov_pr(dst,idx,src)	OP_ADDR(0x89,idx,src,dst)
#define XMov_pc(dst,idx,c)		OP_ADDR(0xC7,idx,dst,0); W(c)
#define XMov_ar(addr,reg)		B(0x3E); if( reg == Eax ) B(0xA3); else OP_RM(0x89,0,reg,5); W(addr)
#define XMov_xr(r,idx,mult,src) OP_RM(0x89,0,src,4); SIB(Mult##mult,idx,r)
#define XCall_r(r)				OP_RM(0xFF,3,2,r)
#define XCall_d(delta)			B(0xE8); W(delta)
#define XPush_r(r)				B(0x50+r)
#define XPush_c(cst)			B(0x68); W(cst)
#define XPush_p(reg,idx)		OP_ADDR(0xFF,idx,6,reg)
#define XAdd_rc(reg,cst)		if IS_SBYTE(cst) { OP_RM(0x83,3,0,reg); B(cst); } else { OP_RM(0x81,3,0,reg); W(cst); }
#define XAdd_rr(dst,src)		OP_RM(0x03,3,dst,src)
#define XSub_rc(reg,cst)		if IS_SBYTE(cst) { OP_RM(0x83,3,5,reg); B(cst); } else { OP_RM(0x81,3,5,reg); W(cst); }
#define XSub_rr(dst,src)		OP_RM(0x2B,3,dst,src)
#define XCmp_rr(r1,r2)			OP_RM(0x3B,3,r1,r2)
#define XCmp_rc(reg,cst)		if( reg == Eax ) B(0x3D); else OP_RM(0x81,3,7,reg); W(cst)
#define XCmp_rb(reg,byte)		OP_RM(0x83,3,7,reg); B(byte)
#define XJump(how,local)		if( how == JAlways ) B(0xE9); else { B(0x0F); B(how); }; local = buf.w; W(0)
#define XJump_r(reg)			OP_RM(0xFF,3,4,reg)
#define XPop_r(reg)				B(0x58 + reg)

#define XTest_rc(r,cst)			if( r == Eax ) { B(0xA9); W(cst); } else { B(0xF7); MOD_RM(3,0,r); W(cst); }
#define XTest_rr(r,src)			B(0x85); MOD_RM(3,r,src)
#define XAnd_rc(r,cst)			if( r == Eax ) { B(0x25); W(cst); } else { B(0x81); MOD_RM(3,4,r); W(cst); }
#define XAnd_rr(r,src)			B(0x23); MOD_RM(3,r,src)
#define XOr_rc(r,cst)			if( r == Eax ) { B(0x0D); W(cst); } else { B(0x81); MOD_RM(3,1,r); W(cst); }
#define XOr_rr(r,src)			B(0x0B); MOD_RM(3,r,src)
#define XXor_rc(r,cst)			if( r == Eax ) { B(0x35); W(cst); } else { B(0x81); MOD_RM(3,6,r); W(cst); }
#define XXor_rr(r,src)			B(0x33); MOD_RM(3,r,src)

#define shift_r(r,spec)			B(0xD3); MOD_RM(3,spec,r);
#define XShl_rr(r,src)			if( src != Ecx ) ERROR; else shift_r(r,4)
#define XShl_rc(r,n)			shift_c(r,n,4)
#define XShr_rr(r,src)			if( src != Ecx ) ERROR; else shift_r(r,5)
#define XShr_rc(r,n)			shift_c(r,n,5)
#define XSar_rr(r,src)			if( src != Ecx ) ERROR; else shift_r(r,7)
#define XSar_rc(r,n)			shift_c(r,n,7)

#define XIMul_rr(dst,src)		B(0x0F); B(0xAF); MOD_RM(3,dst,src)
#define XIDiv_r(r)				B(0xF7); MOD_RM(3,7,r)
#define XCdq()					B(0x99);

// FPU
#define XFAddp()				B(0xDE); B(0xC1)
#define XFSubp()				B(0xDE); B(0xE9)
#define XFMulp()				B(0xDE); B(0xC9)
#define XFDivp()				B(0xDE); B(0xF9)
#define XFStp(r)				B(0xDD); MOD_RM(0,3,r)
#define XFLd(r)					B(0xDD); MOD_RM(0,0,r); if( r == Esp ) B(0x24)
#define XFILd(r)				B(0xDF); MOD_RM(0,5,r); if( r == Esp ) B(0x24)

#define is_int(r,flag,local)	{ XTest_rc(r,1); XJump(flag?JNeq:JEq,local); }

#define stack_pop(r,n) \
	if( n != 0 ) { \
		if( r == CSP ) { \
			XAdd_rc(r,n * 4); \
		} else { \
			XSub_rc(r,n * 4); \
		} \
	}

#define stack_push(r,n) \
	if( n != 0 ) { \
		if( r == CSP ) { \
			XSub_rc(r,n * 4); \
		} else { \
			XAdd_rc(r,n * 4); \
		} \
	}	

#define begin_call()	{ XMov_pr(VM,FIELD(0),SP); XMov_pr(VM,FIELD(1),CSP); }
#define end_call()		{ XMov_rp(SP,VM,FIELD(0)); XMov_rp(CSP,VM,FIELD(1)); }
#define label(code)		{ XMov_rc(TMP,CONST(code));	XCall_r(TMP); }

#define pop(n) if( n != 0 ) { \
		int i = (int)n; \
		while( i-- > 0 ) { \
			XMov_pc(SP,FIELD(i),0); \
		} \
		XSub_rc(SP,(int)(n * 4)); \
	}

#define runtime_error(msg_id,in_label) { \
	XPush_c(CONST(strings[msg_id])); \
	if( in_label ) { \
		XMov_rp(TMP2,Esp,FIELD(2)); \
		XPush_r(TMP2); \
	} else { \
		XPush_c(GET_PC()); \
	} \
	label(code_runtime_error); \
	if( in_label ) { \
		XRet(); \
	} \
}

#define get_var_r(reg,v) { \
	switch( v ) { \
	case VThis: \
		XMov_rp(reg,VM,FIELD(3)); \
		break; \
	case VEnv: \
		XMov_rp(reg,VM,FIELD(2)); \
		break; \
	case VModule: \
		XMov_rc(reg,CONST(ctx->module)); \
		break; \
	case VVm: \
		XMov_rr(reg,VM); \
		break; \
	case VSpMax: \
		XMov_rp(reg,VM,FIELD(5)); \
		break; \
	case VTrap: \
		XMov_rp(reg,VM,FIELD(6)); \
		break; \
	default: \
		ERROR; \
		break; \
	} \
}

#define get_var_p(reg,idx,v) { \
	switch( v ) { \
	case VThis: \
		XMov_rp(TMP,VM,FIELD(3)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VEnv: \
		XMov_rp(TMP,VM,FIELD(2)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VModule: \
		XMov_pc(reg,idx,CONST(ctx->module)); \
		break; \
	case VVm: \
		XMov_pr(reg,idx,VM); \
		break; \
	case VSpMax: \
		XMov_rp(TMP,VM,FIELD(5)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VTrap: \
		XMov_rp(TMP,VM,FIELD(6)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	default: \
		ERROR; \
		break; \
	} \
}

#define set_var_r(v,reg) { \
	switch( v ) { \
	case VThis: \
		XMov_pr(VM,FIELD(3),reg); \
		break; \
	case VEnv: \
		XMov_pr(VM,FIELD(2),reg); \
		break; \
	case VTrap: \
		XMov_pr(VM,FIELD(6),reg); \
		break; \
	case VModule: \
		break; \
	default: \
		ERROR; \
		break; \
	} \
}

#define set_var_p(v,reg,idx) { \
	switch( v ) { \
	case VThis: \
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,FIELD(3),TMP); \
		break; \
	case VEnv: \
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,FIELD(2),TMP); \
		break; \
	case VTrap: \
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,FIELD(6),TMP); \
		break; \
	case VModule: \
		break; \
	default: \
		ERROR; \
		break; \
	} \
}

#define jump(how,targ) { \
	jlist *j = (jlist*)alloc_private(sizeof(jlist)); \
	j->target = (int)((int_val*)targ - ctx->module->code); \
	j->next = ctx->jumps; \
	ctx->jumps = j; \
	XJump(how,j->j); \
}

#define setup_before_call(mode) { \
	push_infos(mode); \
	if( mode != CALLBACK ) { XPush_r(ACC); } \
	if( mode == THIS_CALL ) { \
		set_var_p(VThis,SP,FIELD(0)); \
		pop(1); \
	} \
	set_var_p(VEnv,ACC,FIELD(3)); \
}

#define restore_after_call(nargs) { \
	void *jok; \
	XCmp_rc(ACC,0); \
	XJump(JNeq,jok); \
	XMov_rp(ACC,Esp,FIELD(nargs)); \
	XMov_rp(ACC,ACC,FIELD(4)); \
	XPush_r(ACC); \
	XMov_rc(TMP,CONST(val_throw)); \
	XCall_r(TMP); \
	PATCH_JUMP(jok); \
	stack_pop(Esp,1+nargs); \
	pop_infos(); \
}

static char code_boot[30];
static char code_stack_expand_0[55];
static char code_stack_expand_4[61];
static char code_runtime_error[58];

// calls
#define NARGS (CALL_MAX_ARGS + 1)

static char code_normal_jit[NARGS][125];
static char code_this_jit[NARGS][139];
static char code_tail_jit[NARGS][55];

static char code_normal_prim[NARGS][30];
static char code_this_prim[NARGS][30];
static char code_tail_prim[NARGS][30];

static char code_normal_fun[NARGS][30];
static char code_this_fun[NARGS][30];
static char code_tail_fun[NARGS][30];

static value *strings;
static const char *cstrings[] = {
	"Stack overflow", // 0
	"Reading Outside Env", // 1
	"Writing Outside Env", // 2
	"Invalid call", // 3
};

#define DEFINE_PROC(p,arg) ctx->buf = buf; jit_##p(ctx,arg); buf = ctx->buf
#define push_infos(arg) DEFINE_PROC(push_infos,arg)
#define test(arg)		DEFINE_PROC(test,arg)
#define call(mode,nargs) ctx->buf = buf; jit_call(ctx,mode,(int)nargs); buf = ctx->buf

static jit_ctx *jit_init_context( void *ptr, int size ) {
	jit_ctx *c = (jit_ctx*)alloc_private(sizeof(jit_ctx));
	c->size = size;
	c->baseptr = ptr;
	c->buf.p = ptr;
	c->pos = NULL;
	c->curpc = 0;
	c->jumps = NULL;
	return c;
}

static void jit_finalize_context( jit_ctx *ctx ) {	
	int nbytes = POS();
	if( nbytes == 0 || nbytes > ctx->size )
		*(int*)0xAABBCC = 0;
}

static void jit_push_infos( jit_ctx *ctx, enum Callback callb ) {
	INIT_BUFFER;
	void *jend;
	stack_push(CSP,4);
	XCmp_rr(SP,CSP);
	XJump(JGt,jend);
	label(code_stack_expand_4);
	PATCH_JUMP(jend);
	if( callb == CALLBACK ) {
		XMov_pc(CSP,FIELD(-3),CONST(callback_return));
		get_var_p(CSP,FIELD(-2),VEnv);
		get_var_p(CSP,FIELD(-1),VThis);
		XMov_pc(CSP,FIELD(0),0);		
	} else {
		if( callb == PC_CUR ) {
			XMov_pc(CSP,FIELD(-3),GET_PC());
		} else { // PC_ARG : on the stack
			XMov_rp(TMP2,Esp,FIELD(1));
			XMov_pr(CSP,FIELD(-3),TMP2);			
		}
		get_var_p(CSP,FIELD(-2),VEnv);
		get_var_p(CSP,FIELD(-1),VThis);
		get_var_p(CSP,FIELD(0),VModule)
	}
	END_BUFFER;
}

#define pop_infos() { \
	set_var_p(VModule,CSP,FIELD(0)); \
	set_var_p(VThis,CSP,FIELD(-1)); \
	set_var_p(VEnv,CSP,FIELD(-2)); \
	XMov_pc(CSP,FIELD(0),0); \
	XMov_pc(CSP,FIELD(-1),0); \
	XMov_pc(CSP,FIELD(-2),0); \
	XMov_pc(CSP,FIELD(-3),0); \
	stack_pop(CSP,4); \
}

static void jit_boot( jit_ctx *ctx, void *_ ) {	
	INIT_BUFFER;
	XPush_r(Ebp);
	XPush_r(Ebx);
	XPush_r(Esi);
	XPush_r(Edi);
	XMov_rp(VM,Esp,FIELD(5));
	XMov_rp(TMP,Esp,FIELD(6));
	XMov_rp(ACC,Esp,FIELD(7));
	end_call();
	XCall_r(TMP);
	begin_call();
	XPop_r(Edi);
	XPop_r(Esi);
	XPop_r(Ebx);
	XPop_r(Ebp);
	XRet();
	END_BUFFER;
}

static void jit_stack_expand( jit_ctx *ctx, int n ) {
	INIT_BUFFER;
	void *jok;
	stack_pop(CSP,n);
	XPush_r(ACC);
	XPush_r(VM);
	XPush_r(CSP);
	XPush_r(SP);
	XMov_rc(TMP,CONST(neko_stack_expand));
	XCall_r(TMP);
	XCmp_rb(ACC,0);
	XJump(JNeq,jok);
	XPush_c(CONST(strings[0])); // Stack overflow
	XMov_rc(TMP,CONST(val_throw));
	XCall_r(TMP);
	PATCH_JUMP(jok);
	XMov_rp(ACC,Esp,FIELD(3));
	end_call();
	stack_pop(Esp,4);
	stack_push(CSP,n);
	XRet();
	END_BUFFER;
}

static void jit_runtime_error( jit_ctx *ctx, void *_ ) {
	INIT_BUFFER;
	push_infos(PC_ARG); // pc
	begin_call();
	XMov_rp(TMP,Esp,FIELD(2)); // msg on stack
	XPush_r(TMP);
	XMov_rc(TMP,CONST(val_throw));
	XCall_r(TMP);
	END_BUFFER;
}

static void jit_test( jit_ctx *ctx, int how ) {
	INIT_BUFFER;
	void *jnot1, *jnot2, *jend;
	// call val_compare(sp[0],acc)
	XPush_r(ACC);
	XMov_rp(TMP,SP,FIELD(0));
	XPush_r(TMP);
	begin_call();
	XMov_rc(TMP,CONST(val_compare));
	XCall_r(TMP);
	end_call();
	stack_pop(Esp,2);
	pop(1);
	// test ok and != invalid_comparison
	XCmp_rc(ACC,0);
	XJump(how,jnot1);
	XCmp_rb(ACC,0xFE);
	XJump(JEq,jnot2);
	XMov_rc(ACC,CONST(val_true));
	XJump(JAlways,jend);
	PATCH_JUMP(jnot1);
	PATCH_JUMP(jnot2);
	XMov_rc(ACC,CONST(val_false));
	PATCH_JUMP(jend);
	END_BUFFER;
}

static void jit_call( jit_ctx *ctx, int mode, int nargs ) {
	INIT_BUFFER;
	void *jerr, *jother, *jend1, *jend2, *jerr2, *jend3;

// if( is_int ) : error
	is_int(ACC,true,jerr);

// if( type == jit )
	XMov_rp(TMP,ACC,FIELD(0)); // acc->type
	XCmp_rb(TMP,VAL_JITFUN);
	XJump(JNeq,jother);

	XPush_c(GET_PC());
	switch( mode ) {
	case NORMAL: label(code_normal_jit[nargs]); break;
	case THIS_CALL: label(code_this_jit[nargs]); break;
	case TAIL_CALL: label(code_tail_jit[nargs]); break;
	}

	if( mode == TAIL_CALL )
		jend1 = NULL;
	else {
		XJump(JAlways,jend1);
	}

// else if( type == prim )
	PATCH_JUMP(jother);
	XCmp_rb(TMP,VAL_PRIMITIVE);
	XJump(JNeq,jother);

	XPush_c(GET_PC());
	switch( mode ) {
	case NORMAL: label(code_normal_prim[nargs]); break;
	case THIS_CALL: label(code_this_prim[nargs]); break;
	case TAIL_CALL: label(code_tail_prim[nargs]); break;
	}

	XJump(JAlways,jend2);

// else if( type == function )
	PATCH_JUMP(jother);
	XCmp_rb(TMP,VAL_FUNCTION);
	XJump(JNeq,jerr2);

	XPush_c(GET_PC());
	switch( mode ) {
	case NORMAL: label(code_normal_fun[nargs]); break;
	case THIS_CALL: label(code_this_fun[nargs]); break;
	case TAIL_CALL: label(code_tail_fun[nargs]); break;	
	}
	
	XJump(JAlways,jend3);

// else error
	PATCH_JUMP(jerr);
	PATCH_JUMP(jerr2);
	runtime_error(3,0); // Invalid call

// end
	PATCH_JUMP(jend1);
	PATCH_JUMP(jend2);
	PATCH_JUMP(jend3);
	stack_pop(Esp,1); // pushed pc

	END_BUFFER;
}

void jit_call_jit( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	void *jerr;

	// check arg count	
	XMov_rp(TMP,ACC,FIELD(1));
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jerr);

	if( mode == TAIL_CALL ) {	
		// get the pc from stack and pop eip as well
		XMov_rp(TMP,Esp,FIELD(1));
		stack_pop(Esp,2);		
		// replace PC and Module only on stack
		XMov_pr(CSP,FIELD(-3),TMP);
		get_var_p(CSP,FIELD(0),VModule);

		set_var_p(VModule,ACC,FIELD(4)); // vm->module = acc->module
		set_var_p(VEnv,ACC,FIELD(3)); // vm->env = acc->env
		XMov_rp(TMP,ACC,FIELD(2)); // rtmp = acc->addr
		XJump_r(TMP);		
	} else {
		push_infos(PC_ARG);
		set_var_p(VModule,ACC,FIELD(4)); // vm->module = acc->module
		set_var_p(VEnv,ACC,FIELD(3)); // vm->env = acc->env
		if( mode == THIS_CALL ) {
			set_var_p(VThis,SP,FIELD(0));
			pop(1);
		}
		XMov_rp(TMP,ACC,FIELD(2)); // acc->addr
		XCall_r(TMP);
		pop_infos();
		XRet();
	}
	PATCH_JUMP(jerr);
	runtime_error(3,1); // Invalid call
	END_BUFFER;
}

void jit_call_prim( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	void *jvararg, *jerr;
	int i;

	// check arg count
	XMov_rp(TMP,ACC,FIELD(1)); // acc->nargs
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jvararg);

	// push args from VMSP to PROCSP
	setup_before_call(mode);
	for(i=0;i<nargs;i++) {
		XPush_p(SP,FIELD(i));		
	}
	pop(nargs);

	// call C primitive
	XMov_rp(TMP,ACC,FIELD(2)); // acc->addr
	begin_call();
	XCall_r(TMP);
	end_call();
	restore_after_call(nargs);
	XRet();

//	else if( args == -1 )
	PATCH_JUMP(jvararg);
	XCmp_rb(TMP,-1);
	XJump(JNeq,jerr);	

	// push args from VMSP to PROCSP
	setup_before_call(mode);
	for(i=0;i<nargs;i++) {
		XPush_p(SP,FIELD(i));		
	}
	pop(nargs);

	// push arg ptr and arg count
	XMov_rr(TMP,Esp);
	XPush_c(nargs);
	XPush_r(TMP);

	// call C primitive
	XMov_rp(TMP,ACC,FIELD(2)); // acc->addr
	begin_call();
	XCall_r(TMP);
	end_call();
	restore_after_call(2 + nargs);
	XRet();

// error
	PATCH_JUMP(jerr);
	runtime_error(3,1); // Invalid call
	END_BUFFER;
}

#define jit_call_jit_normal(ctx,i)		jit_call_jit(ctx,i,NORMAL)
#define jit_call_jit_tail(ctx,i)		jit_call_jit(ctx,i,TAIL_CALL)
#define jit_call_jit_this(ctx,i)		jit_call_jit(ctx,i,THIS_CALL)

#define jit_call_prim_normal(ctx,i)		jit_call_prim(ctx,i,NORMAL)
#define jit_call_prim_tail(ctx,i)		jit_call_prim(ctx,i,TAIL_CALL)
#define jit_call_prim_this(ctx,i)		jit_call_prim(ctx,i,THIS_CALL)

#define jit_call_fun_normal(ctx,i)		jit_call_fun(ctx,i,NORMAL)
#define jit_call_fun_tail(ctx,i)		jit_call_fun(ctx,i,TAIL_CALL)
#define jit_call_fun_this(ctx,i)		jit_call_fun(ctx,i,THIS_CALL)

static void jit_opcode( jit_ctx *ctx, enum OPCODE op, int_val p ) {
	INIT_BUFFER;
	void *jok, *jend;
	switch( op ) {
	case AccNull:
		XMov_rc(ACC,CONST(val_null));
		break;
	case AccTrue:
		XMov_rc(ACC,CONST(val_true));
		break;
	case AccFalse:
		XMov_rc(ACC,CONST(val_false));
		break;
	case AccThis:
		get_var_r(ACC,VThis);
		break;
	case AccInt:
		XMov_rc(ACC,CONST(p));
		break;
	case AccStack:
		XMov_rp(ACC,SP,FIELD(p));
		break;
	case AccStack0:
		XMov_rp(ACC,SP,FIELD(0));
		break;
	case AccStack1:
		XMov_rp(ACC,SP,FIELD(1));
		break;
	case AccGlobal:
	case AccBuiltin:
		XMov_ra(ACC,CONST(p));
		break;
	case AccEnv:
		get_var_r(TMP,VEnv);
		XMov_rp(TMP2,TMP,FIELD(0));
		XCmp_rc(TMP2,(p << 3) | VAL_ARRAY);
		XJump(JGt,jok);
		runtime_error(1,0); // Reading Outside Env
		PATCH_JUMP(jok);
		XMov_rp(ACC,TMP,FIELD(p + 1)); // acc = val_array_ptr(env)[p]
		break;	
	case SetStack:
		XMov_pr(SP,FIELD(p),ACC);
		break;
	case SetGlobal:
		XMov_ar(CONST(p),ACC);
		break;
	case SetEnv:	
		get_var_r(TMP,VEnv);
		XMov_rp(TMP2,TMP,FIELD(0));
		XCmp_rc(TMP2,(p << 3) | VAL_ARRAY);
		XJump(JGt,jok);
		runtime_error(2,0); // Writing Outside Env
		PATCH_JUMP(jok);
		XMov_pr(TMP,FIELD(p+1),ACC); // val_array_ptr(env)[p] = acc
		break;
	case SetThis:
		set_var_r(VThis,ACC);
		break;
	case Push:
		stack_push(SP,1);
		XCmp_rr(SP,CSP);
		XJump(JGt,jend);
		label(code_stack_expand_0);
		PATCH_JUMP(jend);
		XMov_pr(SP,FIELD(0),ACC);
		break;
	case Pop:
		pop(p);
		break;
	case Jump:
		jump(JAlways,p);
		break;
	case JumpIf:
		XCmp_rc(ACC,CONST(val_true));
		jump(JEq,p);
		break;
	case JumpIfNot:
		XCmp_rc(ACC,CONST(val_true));
		jump(JNeq,p);
		break;
	case Eq:
		test(JNeq);
		break;
	case Gt:
		test(JSignLte);
		break;
	case Gte:
		test(JSignLt);
		break;
	case Lt:
		test(JSignGte);
		break;
	case Lte:
		test(JSignGt);
		break;
	case Call:
		call(NORMAL,p);
		break;
	case ObjCall:
		call(THIS_CALL,p);
		break;
	default:
		ERROR;
	}
	END_BUFFER;
}


#define MAX_OP_SIZE		64

#define FILL_BUFFER(f,param,ptr) \
	{ \
		jit_ctx *c = jit_init_context(ptr,sizeof(ptr)); \
		f(c,param); \
		jit_finalize_context(c); \
	}

void neko_init_jit() {
	int nstrings = sizeof(cstrings) / sizeof(const char *);
	int i;
	strings = alloc_root(nstrings);
	for(i=0;i<nstrings;i++)
		strings[i] = alloc_string(cstrings[i]);
	FILL_BUFFER(jit_boot,NULL,code_boot);
	FILL_BUFFER(jit_stack_expand,0,code_stack_expand_0);
	FILL_BUFFER(jit_stack_expand,4,code_stack_expand_4);
	FILL_BUFFER(jit_runtime_error,0,code_runtime_error);
	for(i=0;i<NARGS;i++) {
		FILL_BUFFER(jit_call_jit_normal,i,code_normal_jit[i]);
		FILL_BUFFER(jit_call_jit_this,i,code_this_jit[i]);
		FILL_BUFFER(jit_call_jit_tail,i,code_tail_jit[i]);

		FILL_BUFFER(jit_call_prim_normal,i,code_normal_prim[i]);
		FILL_BUFFER(jit_call_prim_this,i,code_this_prim[i]);
		FILL_BUFFER(jit_call_prim_tail,i,code_tail_prim[i]);
/*
		FILL_BUFFER(jit_call_fun_normal,i,code_normal_fun[i]);
		FILL_BUFFER(jit_call_fun_this,i,code_this_fun[i]);
		FILL_BUFFER(jit_call_fun_tail,i,code_tail_fun[i]);
*/	}
}

void neko_free_jit() {
	free_root(strings);
	strings = NULL;
}

void neko_module_jit( neko_module *m ) {
	unsigned int i = 0;
	jit_ctx *ctx = jit_init_context(NULL,0);
	ctx->pos = (int*)alloc_private(sizeof(int)*m->codesize);
	ctx->module = m;
	while( i < m->codesize ) {
		enum OPCODE op = m->code[i];
		int curpos = POS();
		ctx->pos[i] = curpos;
		ctx->curpc = i;
		i++;
		// resize buffer
		if( curpos + MAX_OP_SIZE > ctx->size ) {
			int nsize = ctx->size ? ctx->size * 2 : MAX_OP_SIZE;
			char *buf2 = alloc_private(nsize);
			memcpy(buf2,ctx->baseptr,curpos);
			ctx->baseptr = buf2;
			ctx->buf.p = buf2 + curpos;
			ctx->size = nsize;
		}
		jit_opcode(ctx,op,m->code[i]);
#		ifdef _DEBUG
		if( POS() - curpos > MAX_OP_SIZE )
			ERROR;
#		endif
		i += parameter_table[op];
	}
	jit_finalize_context(ctx);
}

#else // JIT_ENABLE

void neko_init_jit() {
}

void neko_free_jit() {
}

void neko_module_jit( neko_module *m ) {
}


#endif

/* ************************************************************************ */