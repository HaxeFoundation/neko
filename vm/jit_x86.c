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
#include "objtable.h"
#include <string.h>

#if defined(NEKO_X86) && defined(_WIN32) && defined(_DEBUG)
#define JIT_ENABLE
#endif

#ifdef JIT_ENABLE

extern int neko_stack_expand( int_val *sp, int_val *csp, neko_vm *vm );
extern value alloc_module_function( void *m, int_val pos, int nargs );

typedef union {
	void *p;
	unsigned char *b;
	unsigned int *w;
} jit_buffer;

typedef struct _jlist {
	int jpos;
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

#define	MODE_CALLBACK	1
#define MODE_PC_CUR		0
#define MODE_PC_ARG		2

enum PushInfosMode {
	CALLBACK,
	PC_CUR,
	PC_ARG
};

enum CallMode {
	NORMAL,
	THIS_CALL,
	TAIL_CALL,
};

enum Operation {
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
};

enum IOperation {
	IOP_SHL,
	IOP_SHR,
	IOP_USHR,
	IOP_AND,
	IOP_OR,
	IOP_XOR,
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
#define CONST(v)				((int)(int_val)(v))
#define PATCH_JUMP(local)		if( local != NULL ) *(int*)local = (int)((int_val)buf.p - ((int_val)local + 4))
#define FIELD(n)				((n) * 4)
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
#define XMov_rp(dst,reg,idx)	OP_ADDR(0x8B,idx,reg,dst)
#define XMov_ra(dst,addr)		OP_RM(0x8B,0,dst,5); W(addr)
#define XMov_rx(dst,r,idx,mult) OP_RM(0x8B,0,dst,4); SIB(Mult##mult,idx,r)
#define XMov_pr(dst,idx,src)	OP_ADDR(0x89,idx,dst,src)
#define XMov_pc(dst,idx,c)		OP_ADDR(0xC7,idx,dst,0); W(c)
#define XMov_ar(addr,reg)		B(0x3E); if( reg == Eax ) { B(0xA3); } else { OP_RM(0x89,0,reg,5); }; W(addr)
#define XMov_xr(r,idx,mult,src) OP_RM(0x89,0,src,4); SIB(Mult##mult,idx,r)
#define XCall_r(r)				OP_RM(0xFF,3,2,r)
#define XCall_d(delta)			B(0xE8); W(delta)
#define XPush_r(r)				B(0x50+r)
#define XPush_c(cst)			B(0x68); W(cst)
#define XPush_p(reg,idx)		OP_ADDR(0xFF,idx,reg,6)
#define XAdd_rc(reg,cst)		if IS_SBYTE(cst) { OP_RM(0x83,3,0,reg); B(cst); } else { OP_RM(0x81,3,0,reg); W(cst); }
#define XAdd_rr(dst,src)		OP_RM(0x03,3,dst,src)
#define XSub_rc(reg,cst)		if IS_SBYTE(cst) { OP_RM(0x83,3,5,reg); B(cst); } else { OP_RM(0x81,3,5,reg); W(cst); }
#define XSub_rr(dst,src)		OP_RM(0x2B,3,dst,src)
#define XCmp_rr(r1,r2)			OP_RM(0x3B,3,r1,r2)
#define XCmp_rc(reg,cst)		if( reg == Eax ) { B(0x3D); } else { OP_RM(0x81,3,7,reg); }; W(cst)
#define XCmp_rb(reg,byte)		OP_RM(0x83,3,7,reg); B(byte)
#define XJump(how,local)		if( (how) == JAlways ) { B(0xE9); } else { B(0x0F); B(how); }; local = buf.w; W(0)
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
#define shift_c(r,n,spec)		if( (n) == 1 ) { B(0xD1); MOD_RM(3,spec,r); } else { B(0xC1); MOD_RM(3,spec,r); B(n); }

#define XShl_rr(r,src)			if( src != Ecx ) ERROR; shift_r(r,4)
#define XShl_rc(r,n)			shift_c(r,n,4)
#define XShr_rr(r,src)			if( src != Ecx ) ERROR; shift_r(r,5)
#define XShr_rc(r,n)			shift_c(r,n,5)
#define XSar_rr(r,src)			if( src != Ecx ) ERROR; shift_r(r,7)
#define XSar_rc(r,n)			shift_c(r,n,7)

#define XIMul_rr(dst,src)		B(0x0F); B(0xAF); MOD_RM(3,dst,src)
#define XIDiv_r(r)				B(0xF7); MOD_RM(3,7,r)
#define XCdq()					B(0x99);

// FPU
#define XFAddp()				B(0xDE); B(0xC1)
#define XFSubp()				B(0xDE); B(0xE9)
#define XFMulp()				B(0xDE); B(0xC9)
#define XFDivp()				B(0xDE); B(0xF9)
#define XFStp(r)				B(0xDD); MOD_RM(0,3,r); if( r == Esp ) B(0x24)
#define XFLd(r)					B(0xDD); MOD_RM(0,0,r); if( r == Esp ) B(0x24)
#define XFILd(r)				B(0xDF); MOD_RM(0,5,r); if( r == Esp ) B(0x24)

#define is_int(r,flag,local)	{ XTest_rc(r,1); XJump((flag)?JNeq:JEq,local); }

#define stack_push(r,n) \
	if( (n) != 0 ) { \
		if( (r) == CSP ) { \
			XAdd_rc(r,(n) * 4); \
		} else { \
			XSub_rc(r,(n) * 4); \
		} \
	}

#define stack_pop(r,n) \
	if( (n) != 0 ) { \
		if( (r) == CSP ) { \
			XSub_rc(r,(n) * 4); \
		} else { \
			XAdd_rc(r,(n) * 4); \
		} \
	}

#define begin_call()	{ XMov_pr(VM,FIELD(0),SP); XMov_pr(VM,FIELD(1),CSP); }
#define end_call()		{ XMov_rp(SP,VM,FIELD(0)); XMov_rp(CSP,VM,FIELD(1)); }
#define label(code)		{ XMov_rc(TMP,CONST(code));	XCall_r(TMP); }

#define todo(str)		{ int *loop; XMov_rc(TMP,CONST(str)); XJump(JAlways,loop); *loop = -5; }

#define pop(n) if( (n) != 0 ) { \
		int i = (n); \
		while( i-- > 0 ) { \
			XMov_pc(SP,FIELD(i),0); \
		} \
		stack_pop(SP,n); \
	}

#define runtime_error(msg_id,in_label) { \
	XPush_c(CONST(strings[msg_id])); \
	if( in_label ) { \
		XMov_rp(TMP2,Esp,FIELD(2)); \
		XPush_r(TMP2); \
	} else { \
		XPush_c(GET_PC()); \
	} \
	label(code->runtime_error); \
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
		XMov_rp(reg,VM,FIELD(7)); \
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
		XMov_rp(TMP,VM,FIELD(7)); \
		XMov_pr(reg,idx,TMP); \
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
		XMov_pr(VM,FIELD(7),reg); \
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
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,FIELD(7),TMP); \
		break; \
	default: \
		ERROR; \
		break; \
	} \
}

#define jump(how,targ) { \
	jlist *j = (jlist*)alloc(sizeof(jlist)); \
	void *jcode; \
	j->target = (int)((int_val*)(int_val)(targ) - ctx->module->code); \
	j->next = ctx->jumps; \
	ctx->jumps = j; \
	XJump(how,jcode); \
	j->jpos = (int)((int_val)jcode - (int_val)ctx->baseptr); \
}

#define setup_before_call(mode,is_callb) { \
	push_infos(is_callb?CALLBACK:PC_ARG); \
	if( !is_callb ) { XPush_r(ACC); } \
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

#define NARGS (CALL_MAX_ARGS + 1)
#define MAX_ENV		8

typedef struct {
	char *boot;
	char *stack_expand_0;
	char *stack_expand_4;
	char *runtime_error;
	char *call_normal_jit[NARGS];
	char *call_this_jit[NARGS];
	char *call_tail_jit[NARGS];
	char *call_normal_prim[NARGS];
	char *call_this_prim[NARGS];
	char *call_tail_prim[NARGS];
	char *call_normal_fun[NARGS];
	char *call_this_fun[NARGS];
	char *call_tail_fun[NARGS];
	char *make_env[MAX_ENV];
} jit_code;

char *jit_boot_seq = NULL;
static jit_code *code;

static value *strings;
static const char *cstrings[] = {
	"Stack overflow", // 0
	"Reading Outside Env", // 1
	"Writing Outside Env", // 2
	"Invalid call", // 3
	"Invalid array access", // 4
	"Invalid field access", // 5
	"Invalid environment", // 6
	"+", // 7
	"-", // 8
	"*", // 9
	"/", // 10
	"<<", // 11
	">>", // 12
	">>>", // 13
	"&", // 14
	"|", // 15
	"^", // 16
};

#define DEFINE_PROC(p,arg) ctx->buf = buf; jit_##p(ctx,arg); buf = ctx->buf
#define push_infos(arg) DEFINE_PROC(push_infos,arg)
#define test(arg)		DEFINE_PROC(test,arg)
#define call(mode,nargs) ctx->buf = buf; jit_call(ctx,mode,nargs); buf = ctx->buf
#define number_op(arg)	DEFINE_PROC(number_op,arg)
#define array_access(p)	DEFINE_PROC(array_access,p)
#define int_op(arg)		DEFINE_PROC(int_op,arg)

static jit_ctx *jit_init_context( void *ptr, int size ) {
	jit_ctx *c = (jit_ctx*)alloc(sizeof(jit_ctx));
	c->size = size;
	c->baseptr = ptr;
	c->buf.p = ptr;
	c->pos = NULL;
	c->curpc = 0;
	c->jumps = NULL;
	return c;
}

static void jit_finalize_context( jit_ctx *ctx ) {
	jlist *l;
	int nbytes = POS();
	if( nbytes == 0 || nbytes > ctx->size )
		*(int*)0xAABBCC = 0;
	l = ctx->jumps;
	while( l != NULL ) {
		*(int*)((char*)ctx->baseptr + l->jpos) = ctx->pos[l->target] - (l->jpos + 4);
		l = l->next;
	}
}

static void jit_push_infos( jit_ctx *ctx, enum Callback callb ) {
	INIT_BUFFER;
	void *jend;
	stack_push(CSP,4);
	XCmp_rr(SP,CSP);
	XJump(JGt,jend);
	label(code->stack_expand_4);
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
	set_var_p(VModule,Esp,FIELD(8));
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
	is_int(ACC,1,jerr);

// if( type == jit )
	XMov_rp(TMP,ACC,FIELD(0)); // acc->type
	XCmp_rb(TMP,VAL_JITFUN);
	XJump(JNeq,jother);

	XPush_c(GET_PC());
	switch( mode ) {
	case NORMAL: label(code->call_normal_jit[nargs]); break;
	case THIS_CALL: label(code->call_this_jit[nargs]); break;
	case TAIL_CALL: label(code->call_tail_jit[nargs]); break;
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
	case NORMAL: label(code->call_normal_prim[nargs]); break;
	case THIS_CALL: label(code->call_this_prim[nargs]); break;
	case TAIL_CALL: label(code->call_tail_prim[nargs]); break;
	}

	XJump(JAlways,jend2);

// else if( type == function )
	PATCH_JUMP(jother);
	XCmp_rb(TMP,VAL_FUNCTION);
	XJump(JNeq,jerr2);

	XPush_c(GET_PC());
	switch( mode ) {
	case NORMAL: label(code->call_normal_fun[nargs]); break;
	case THIS_CALL: label(code->call_this_fun[nargs]); break;
	case TAIL_CALL: label(code->call_tail_fun[nargs]); break;
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

static void jit_call_jit( jit_ctx *ctx, int nargs, int mode ) {
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

static void jit_call_prim( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	void *jvararg, *jerr;
	int i;

	// check arg count
	XMov_rp(TMP,ACC,FIELD(1)); // acc->nargs
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jvararg);

	// push args from VMSP to PROCSP
	setup_before_call(mode,0);
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
	setup_before_call(mode,0);
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

static void jit_call_fun( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	void *jerr;

	// check arg count
	XMov_rp(TMP,ACC,FIELD(1));
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jerr);

	// C call : neko_interp(vm,m,acc,pc)
	setup_before_call(mode,1);
	stack_push(Esp,4);
	XMov_rp(TMP,ACC,FIELD(2)); // acc->addr
	XMov_pr(Esp,FIELD(3),TMP);
	XMov_pr(Esp,FIELD(2),ACC);
	get_var_p(Esp,FIELD(1),VModule);
	get_var_p(Esp,FIELD(0),VVm);
	XMov_rc(TMP,CONST(neko_interp));
	begin_call();
	XCall_r(TMP);
	end_call();
	stack_pop(Esp,4);
	XRet();

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

static void jit_number_op( jit_ctx *ctx, enum OPERATION op ) {
	INIT_BUFFER;
	void *jnot_int1, *jnot_int2, *jfloat1, *jfloat2, *jfloat3, *jint, *jnext;
	void *jerr1, *jerr2, *jerr3, *jerr4, *jerr5, *jend, *jend2, *jend3;

	// acc <=> sp
	XMov_rr(TMP,ACC);
	XMov_rp(ACC,SP,FIELD(0));

	// is_int(acc) && is_int(sp)
	is_int(ACC,false,jnot_int1);
	is_int(TMP,false,jnot_int2);

	XSar_rc(ACC,1);
	XSar_rc(TMP,1);

	if( op != OP_DIV ) {
		switch( op ) {
		case OP_ADD:
			XAdd_rr(ACC,TMP);
			break;
		case OP_SUB:
			XSub_rr(ACC,TMP);
			break;
		case OP_MUL:
			XIMul_rr(ACC,TMP);
			break;
		default:
			ERROR;
			break;
		}
		XShl_rc(ACC,1);
		XOr_rc(ACC,1);
	}
	XJump(JAlways,jend);

	// is_int(acc) && is_number(sp)
	PATCH_JUMP(jnot_int2);
	XMov_rp(TMP2,TMP,FIELD(0));
	XCmp_rb(TMP2,1);

	XJump(JNeq,jerr1);
	XSar_rc(ACC,1);
	XPush_c(0);
	XPush_r(ACC);
	XFILd(Esp);
	XAdd_rc(TMP,8);
	XFLd(TMP);
	stack_pop(Esp,2);
	XJump(JAlways,jfloat1);

	// is_number(acc) ?
	PATCH_JUMP(jnot_int1);
	XMov_rp(TMP2,ACC,FIELD(0));
	XCmp_rb(TMP2,1);
	XJump(JNeq,jerr2);

	// is_number(acc) && is_number(sp)
	is_int(TMP,true,jint);
	XMov_rp(TMP2,TMP,FIELD(0));
	XCmp_rc(TMP2,1);
	XJump(JNeq,jerr3);
	XAdd_rc(ACC,8);
	XFLd(ACC);
	XAdd_rc(TMP,8);
	XFLd(TMP);
	XJump(JAlways,jfloat2);

	// is_number(acc) && is_int(sp)
	PATCH_JUMP(jint);
	XAdd_rc(ACC,8);
	XFLd(ACC);
	XSar_rc(TMP,1);
	XPush_c(0);
	XPush_r(TMP);
	XFILd(Esp);
	stack_pop(Esp,2);
	XJump(JAlways,jfloat3);

	// is_object(acc) ?
	PATCH_JUMP(jerr2);
	XCmp_rb(TMP2,VAL_OBJECT);
	XJump(JNeq,jnext);
	todo("object op 1");
	XJump(JAlways,jend2);

	// is_object(sp) ?
	PATCH_JUMP(jnext);
	is_int(TMP,true,jerr4);
	XMov_rp(TMP2,TMP,FIELD(0));
	PATCH_JUMP(jerr1);
	PATCH_JUMP(jerr3);
	XCmp_rb(TMP2,VAL_OBJECT);
	XJump(JNeq,jerr5);
	todo("object op 2");
	XJump(JAlways,jend3);

	// error
	PATCH_JUMP(jerr5);
	PATCH_JUMP(jerr4);
	runtime_error(7 + op,false);

	// division is always float
	if( op == OP_DIV ) {
		PATCH_JUMP(jend);
		XPush_c(0);
		XPush_r(ACC);
		XFILd(Esp);
		stack_pop(Esp,1);
		XPush_r(TMP);
		XFILd(Esp);
		stack_pop(Esp,2);
	}

	// perform operation
	PATCH_JUMP(jfloat1);
	PATCH_JUMP(jfloat2);
	PATCH_JUMP(jfloat3);

	switch( op ) {
	case OP_ADD:
		XFAddp();
		break;
	case OP_SUB:
		XFSubp();
		break;
	case OP_DIV:
		XFDivp();
		break;
	case OP_MUL:
		XFMulp();
		break;
	default:
		ERROR;
		break;
	}

	stack_push(Esp,2);
	XFStp(Esp);
	XMov_rc(TMP,CONST(alloc_float));
	XCall_r(TMP);
	stack_pop(Esp,2);

	if( op != OP_DIV ) PATCH_JUMP(jend);
	pop(1);
	END_BUFFER;
}

static void jit_int_op( jit_ctx *ctx, enum IOperation op ) {
	INIT_BUFFER;
	void *jerr1, *jerr2, *jend;

	is_int(ACC,false,jerr1);
	XMov_rr(TMP,ACC);
	XSar_rc(TMP,1);
	XMov_rp(ACC,SP,FIELD(0));
	
	is_int(ACC,false,jerr2);
	XSar_rc(ACC,1);

	switch( op ) {
	case IOP_SHL:
		XShl_rr(ACC,TMP);
		break;
	case IOP_SHR:
		XShr_rr(ACC,TMP);
		break;
	case IOP_USHR:
		XSar_rr(ACC,TMP);
		break;
	case IOP_AND:
		XAnd_rr(ACC,TMP);
		break;
	case IOP_OR:
		XOr_rr(ACC,TMP);
		break;
	case IOP_XOR:
		XXor_rr(ACC,TMP);
		break;
	default:
		ERROR;
	}
	
	XShl_rc(ACC,1);
	XOr_rc(ACC,1);
	XJump(JAlways,jend);
	
	PATCH_JUMP(jerr1);
	PATCH_JUMP(jerr2);
	runtime_error(11 + op,false);
	PATCH_JUMP(jend);
	pop(1);

	END_BUFFER;
}

static void jit_array_access( jit_ctx *ctx, int n ) {
	INIT_BUFFER;
	void *jerr1, *jerr2, *jend1, *jend2, *jend3;
	void *jnot_array, *jbounds;

	is_int(ACC,true,jerr1);
	XMov_rp(TMP,ACC,0);
	XMov_rr(TMP2,TMP);
	XAnd_rc(TMP2,7);
	XCmp_rb(TMP2,VAL_ARRAY);

	XJump(JNeq,jnot_array);
	XShr_rc(TMP,3);
	XCmp_rc(TMP,n);
	XJump(JLte,jbounds);
	XMov_rp(ACC,ACC,FIELD(n + 1));
	XJump(JAlways,jend1);

	PATCH_JUMP(jbounds);
	XMov_rc(ACC,CONST(val_null));
	XJump(JAlways,jend2);

	PATCH_JUMP(jnot_array);
	XCmp_rb(TMP2,VAL_OBJECT);
	XJump(JNeq,jerr2);
	todo("Object Array Access");
	XJump(JAlways,jend3);
	PATCH_JUMP(jerr1);
	PATCH_JUMP(jerr2);
	runtime_error(4,false);
	PATCH_JUMP(jend1);
	PATCH_JUMP(jend2);
	PATCH_JUMP(jend3);
	END_BUFFER;
}

static void jit_make_env( jit_ctx *ctx, int esize ) {
	INIT_BUFFER;
	void *jerr1, *jerr2, *jok;
	int i;

	// check type t_function or t_jit
	is_int(ACC,true,jerr1);
	XMov_rp(TMP,ACC,FIELD(0)); // acc->type
	XCmp_rb(TMP,VAL_JITFUN);
	XJump(JEq,jok);
	XCmp_rb(TMP,VAL_FUNCTION);
	XJump(JNeq,jerr2);
	PATCH_JUMP(jok);

	// prepare args for alloc_module_function
	XPush_r(TMP);				// acc->type
	stack_push(Esp,1);			// empty cell
	XMov_rp(TMP,ACC,FIELD(1));	// acc->nargs
	XPush_r(TMP);
	XMov_rp(TMP,ACC,FIELD(2));  // acc->addr
	XPush_r(TMP);
	XMov_rp(TMP,ACC,FIELD(4));  // acc->module
	XPush_r(TMP);

	// call alloc_array(n)
	XPush_c(esize);
	XMov_rc(TMP,CONST(alloc_array));
	XCall_r(TMP);
	stack_pop(Esp,1);

	// fill array
	for(i=0;i<esize;i++) {
		XMov_rp(TMP,SP,FIELD(i));
		XMov_pr(ACC,FIELD(esize-i),TMP);
	}
	pop(esize);

	// call alloc_module_function
	XMov_pr(Esp,FIELD(3),ACC); // save acc
	XMov_rc(TMP,CONST(alloc_module_function));
	XCall_r(TMP);
	XMov_rp(TMP,Esp,FIELD(3)); // restore acc
	XMov_rp(TMP2,Esp,FIELD(4)); // restore type
	stack_pop(Esp,5);
	XMov_pr(ACC,FIELD(0),TMP2); // acc->type = type
	XMov_pr(ACC,FIELD(3),TMP);  // acc->env = env
	XRet();

	// errors
	PATCH_JUMP(jerr1);
	PATCH_JUMP(jerr2);
	runtime_error(6,true); // Invalid environment
	END_BUFFER;
}

static void jit_opcode( jit_ctx *ctx, enum OPCODE op, int p ) {
	INIT_BUFFER;
	int i;
	void *jok;
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
	case AccBuiltin:
		XMov_rc(ACC,CONST(p));
		break;
	case AccGlobal:
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
	case AccArray: {
		void *jerr1, *jerr2, *jerr3, *jnot_array, *jbounds, *jend1, *jend2, *jend3;

		// check array & int
		XMov_rp(TMP,SP,FIELD(0));
		pop(1);
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XAnd_rc(TMP2,7);
		XCmp_rb(TMP2,VAL_ARRAY);
		XJump(JNeq,jnot_array);
		is_int(ACC,false,jerr2);

		// check bounds & access array
		XSar_rc(ACC,1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XShr_rc(TMP2,3);
		XCmp_rr(ACC,TMP2);
		XJump(JGte,jbounds);
		XAdd_rc(ACC,1);			  // acc = val_array_ptr(tmp)[acc]
		XMov_rx(ACC,TMP,ACC,4);
		XJump(JAlways,jend1);

		// outside bounds
		PATCH_JUMP(jbounds);
		XMov_rc(ACC,CONST(val_null));
		XJump(JAlways,jend2);

		// check object
		PATCH_JUMP(jnot_array);
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr3);
		todo("Object Array Access");
		XJump(JAlways,jend3);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		PATCH_JUMP(jerr3);
		runtime_error(4,false); // Invalid array access
		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
		break;
		}
	case AccIndex:
		array_access(p);
		break;
	case AccIndex0:
		array_access(0);
		break;
	case AccIndex1:
		array_access(1);
		break;
	case AccField: {
		void *jerr1, *jerr2, *jend1, *jend2;
		is_int(ACC,true,jerr1);
		XMov_rp(TMP,ACC,FIELD(0));
		XCmp_rb(TMP,VAL_OBJECT);
		XJump(JNeq,jerr2);
		XPush_c(p);
		XMov_rp(TMP,ACC,FIELD(1));
		XPush_r(TMP);
		XMov_rc(TMP,CONST(otable_find));
		XCall_r(TMP);
		stack_pop(Esp,2);
		XCmp_rc(ACC,0);
		XJump(JNeq,jend1);
		XMov_rc(ACC,CONST(val_null));
		XJump(JAlways,jend2);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(5,false); // Invalid field access

		PATCH_JUMP(jend1);
		XMov_rp(ACC,ACC,FIELD(0));
		PATCH_JUMP(jend2);
		break;
		}
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
	case SetField: {
		void *jerr1, *jerr2, *jend;
		XMov_rp(TMP,SP,FIELD(0));
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr2);

		// call otable_replace(table,field,acc)
		XPush_r(ACC);
		XPush_c(p);
#		ifdef COMPACT_TABLE
		XMov_rp(TMP,TMP,FIELD(1));
		XPush_r(TMP);
		XMov_rc(TMP,CONST(otable_replace));
#		else
		XAdd_rc(TMP,4);	// pass the address as parameter
		XPush_r(TMP);
		XMov_rc(TMP,CONST(_otable_replace));
#		endif
		XCall_r(TMP);
		stack_pop(Esp,3);
		XMov_rp(ACC,Esp,FIELD(-1));
		XJump(JAlways,jend);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(5,false);
		PATCH_JUMP(jend);
		pop(1);
		break;
		}
	case SetArray: {
		void *jerr1, *jerr2, *jerr3, *jnot_array, *jend1, *jend2, *jend3;
		XMov_rp(TMP,SP,FIELD(0)); // sp[0] : array/object
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XAnd_rc(TMP2,7);
		XCmp_rb(TMP2,VAL_ARRAY);
		XJump(JNeq,jnot_array);

		XMov_rp(TMP2,SP,FIELD(1)); // sp[1] : index
		is_int(TMP2,false,jerr2);

		XMov_rp(TMP,TMP,FIELD(0)); // tmp = tmp->type
		XSar_rc(TMP2,1);
		XShr_rc(TMP,3);
		XCmp_rr(TMP2,TMP);
		XJump(JGte,jend1);

		XMov_rp(TMP,SP,FIELD(0));
		XAdd_rc(TMP2,1);
		XMov_xr(TMP,TMP2,4,ACC);
		XJump(JAlways,jend2);

		PATCH_JUMP(jnot_array);
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr3);
		todo("Object Array Access");
		XJump(JAlways,jend3);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		PATCH_JUMP(jerr3);
		runtime_error(4,false);

		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
		pop(2);
		break;
		}
	case SetIndex: {
		void *jerr1, *jerr2, *jnot_array, *jend1, *jend2, *jend3;
		XMov_rp(TMP,SP,FIELD(0)); // sp[0] : array / object
		pop(1);
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XAnd_rc(TMP2,7);
		XCmp_rb(TMP2,VAL_ARRAY);
		XJump(JNeq,jnot_array);

		XMov_rp(TMP2,TMP,FIELD(0));
		XCmp_rc(TMP2,(p << 3) | VAL_ARRAY); // fake header
		XJump(JLte,jend1);
		XMov_pr(TMP,FIELD(p + 1),ACC);
		XJump(JAlways,jend2);

		PATCH_JUMP(jnot_array);
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr2);
		todo("Object Array Access");
		XJump(JAlways,jend3);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(4,false);

		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
		break;
		}
	case Push: {
		void *jend;
		stack_push(SP,1);
		XCmp_rr(SP,CSP);
		XJump(JGt,jend);
		label(code->stack_expand_0);
		PATCH_JUMP(jend);
		XMov_pr(SP,FIELD(0),ACC);
		break;
		}
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
	case Neq: {
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
		// test if ok and != invalid_comparison
		XCmp_rc(ACC,0);
		XJump(JNeq,jnot1);
		XCmp_rb(ACC,0xFE);
		XJump(JEq,jnot2);
		XMov_rc(ACC,CONST(val_false));
		XJump(JAlways,jend);
		PATCH_JUMP(jnot1);
		PATCH_JUMP(jnot2);
		XMov_rc(ACC,CONST(val_true));
		PATCH_JUMP(jend);
		break;
		}
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
	case Bool:
	case Not: {
		void *jfalse1, *jfalse2, *jfalse3, *jend;
		XCmp_rc(ACC,CONST(val_false));
		XJump(JEq,jfalse1);
		XCmp_rc(ACC,CONST(val_null));
		XJump(JEq,jfalse2);
		XCmp_rc(ACC,CONST(alloc_int(0)));
		XJump(JEq,jfalse3);
		XMov_rc(ACC,CONST((op == Bool)?val_true:val_false));
		XJump(JAlways,jend);
		PATCH_JUMP(jfalse1);
		PATCH_JUMP(jfalse2);
		PATCH_JUMP(jfalse3);
		XMov_rc(ACC,CONST((op == Bool)?val_false:val_true));
		PATCH_JUMP(jend);
		break;
		}
	case IsNull: {
		void *jnext, *jend;
		XCmp_rc(ACC,CONST(val_null));
		XJump(JNeq,jnext);
		XMov_rc(ACC,CONST(val_true));
		XJump(JAlways,jend);
		PATCH_JUMP(jnext);
		XMov_rc(ACC,CONST(val_false));
		PATCH_JUMP(jend);
		break;
		}
	case IsNotNull: {
		void *jnext, *jend;
		XCmp_rc(ACC,CONST(val_null));
		XJump(JNeq,jnext);
		XMov_rc(ACC,CONST(val_false));
		XJump(JAlways,jend);
		PATCH_JUMP(jnext);
		XMov_rc(ACC,CONST(val_true));
		PATCH_JUMP(jend);
		break;
		}
	case Call:
		call(NORMAL,p);
		break;
	case ObjCall:
		call(THIS_CALL,p);
		break;
	case TailCall:
		{
			int stack = (p >> 3);
			int nargs = (p & 7);
			int i = nargs;
			while( i > 0 ) {
				i--;
				XMov_rp(TMP,SP,FIELD(i));
				XMov_pr(SP,FIELD(stack - nargs + i),TMP);
			}
			pop(stack - nargs);
			call(TAIL_CALL,nargs);
			// in case we return from a Primitive
			XRet();
		}
		break;
	case Ret:
		pop(p);
		XRet();
		break;
	case Add:
		// COMPLETE !
		number_op(OP_ADD);
		break;
	case Sub:
		number_op(OP_SUB);
		break;
	case Div:
		number_op(OP_DIV);
		break;
	case Mult:
		number_op(OP_MUL);
		break;
	case Shl:
		int_op(IOP_SHL);
		break;
	case Shr:
		int_op(IOP_SHR);
		break;
	case UShr:
		int_op(IOP_USHR);
		break;
	case And:
		int_op(IOP_AND);
		break;
	case Or:
		int_op(IOP_OR);
		break;
	case Xor:
		int_op(IOP_XOR);
		break;
	case New:
		XPush_r(ACC);
		XMov_rc(TMP,CONST(alloc_object));
		XCall_r(TMP);
		stack_pop(Esp,1);
		break;
	case MakeArray:
		XPush_r(ACC);
		XPush_c(p + 1);
		XMov_rc(TMP,CONST(alloc_array));
		XCall_r(TMP);
		XMov_rp(TMP,Esp,FIELD(1)); // tmp = saved acc
		XMov_pr(ACC,FIELD(1),TMP); // val_array_ptr(acc)[0] = tmp
		stack_pop(Esp,2);
		i = 0;
		while( p > 0 ) {
			p--;
			i++;
			XMov_rp(TMP,SP,FIELD(p));
			XMov_pr(ACC,FIELD(i + 1),TMP);
			XMov_pc(SP,FIELD(p),0);
		}
		stack_pop(SP,i);
		break;
	case MakeEnv:
		XPush_c(GET_PC());
		if( p >= MAX_ENV )
			ERROR;
		label(code->make_env[p]);
		stack_pop(Esp,1);
		break;
	case Last:
		XRet();
		break;
	case Trap:
	case EndTrap:
	case Mod:
	case TypeOf:
	case Compare:
	case Hash:
	case JumpTable:
	case Apply:
	case PhysCompare:
	default:
		ERROR;
	}
	END_BUFFER;
}


#define MAX_OP_SIZE		272

#define FILL_BUFFER(f,param,ptr,size) \
	{ \
		jit_ctx *c; \
		code->ptr = (char*)alloc_private(size); \
		c = jit_init_context(code->ptr,size); \
		f(c,param); \
		jit_finalize_context(c); \
	}

void neko_init_jit() {
	int nstrings = sizeof(cstrings) / sizeof(const char *);
	int i,delta;
	strings = alloc_root(nstrings);
	for(i=0;i<nstrings;i++)
		strings[i] = alloc_string(cstrings[i]);
	code = (jit_code*)alloc_root(sizeof(jit_code) / sizeof(char*));
	FILL_BUFFER(jit_boot,NULL,boot,330);
	FILL_BUFFER(jit_stack_expand,0,stack_expand_0,450);
	FILL_BUFFER(jit_stack_expand,4,stack_expand_4,510);
	FILL_BUFFER(jit_runtime_error,0,runtime_error,600);
	for(i=0;i<NARGS;i++) {
		FILL_BUFFER(jit_call_jit_normal,i,call_normal_jit[i],1270);
		FILL_BUFFER(jit_call_jit_this,i,call_this_jit[i],1410);
		FILL_BUFFER(jit_call_jit_tail,i,call_tail_jit[i],570);

		delta = (i?3:0) + 20 * i;
		FILL_BUFFER(jit_call_prim_normal,i,call_normal_prim[i],3150 + delta);
		FILL_BUFFER(jit_call_prim_this,i,call_this_prim[i],3430 + delta);
		FILL_BUFFER(jit_call_prim_tail,i,call_tail_prim[i],3150 + delta);

		FILL_BUFFER(jit_call_fun_normal,i,call_normal_fun[i],1240);
		FILL_BUFFER(jit_call_fun_this,i,call_this_fun[i],1380);
		FILL_BUFFER(jit_call_fun_tail,i,call_tail_fun[i],1240);
	}
	for(i=0;i<MAX_ENV;i++) {
		FILL_BUFFER(jit_make_env,i,make_env[i],1080);
	}
	jit_boot_seq = code->boot;
}

void neko_free_jit() {
	free_root((value*)code);
	free_root(strings);
	code = NULL;
	strings = NULL;
	jit_boot_seq = NULL;
}

void neko_module_jit( neko_module *m ) {
	unsigned int i = 0;
	jit_ctx *ctx = jit_init_context(NULL,0);
	ctx->pos = (int*)alloc_private(sizeof(int)*(m->codesize + 1));
	ctx->module = m;
	while( i <= m->codesize ) {
		enum OPCODE op = m->code[i];
		int curpos = POS();
		ctx->pos[i] = curpos;
		ctx->curpc = i + 2;
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
		jit_opcode(ctx,op,(int)m->code[i]);
#		ifdef _DEBUG
		{
			int bytes = POS() - curpos;
			if( bytes > MAX_OP_SIZE )
				ERROR;
		}
#		endif
		i += parameter_table[op];
	}
	// FINALIZE
	{
		int csize = POS();
		char *rbuf = alloc_private(csize);
		memcpy(rbuf,ctx->baseptr,csize);
		ctx->baseptr = rbuf;
		ctx->buf.p = rbuf + csize;
		ctx->size = csize;
		jit_finalize_context(ctx);
	}
	// UPDATE GLOBALS
	{
		for(i=0;i<m->nglobals;i++) {
			vfunction *f = (vfunction*)m->globals[i];
			if( !val_is_int(f) && val_tag(f) == VAL_FUNCTION && f->module == m ) {
				int pc = (int)((int_val*)f->addr - m->code);
				f->t = VAL_JITFUN;
				f->addr = (char*)ctx->baseptr + ctx->pos[pc];
			}
		}
	}
	m->jit = ctx->baseptr;
}

#else // JIT_ENABLE

char *jit_boot_seq = NULL;

void neko_init_jit() {
}

void neko_free_jit() {
}

void neko_module_jit( neko_module *m ) {
}


#endif

/* ************************************************************************ */