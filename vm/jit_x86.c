/*
 * Copyright (C)2005-2017 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "vm.h"
#include "neko_mod.h"
#include "objtable.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifdef NEKO_POSIX
#	include <sys/types.h>
#	include <sys/mman.h>
#	define USE_MMAP
#endif

#define tmp_alloc(size) malloc(size)
#define tmp_free(ptr)	free(ptr)

#ifdef NEKO_MAC
#define STACK_ALIGN
#endif

#if defined(NEKO_WINDOWS) && defined(_DEBUG_XX)
#define	STACK_ALIGN
#define STACK_ALIGN_DEBUG
#endif

#define TAG_MASK		((1<<NEKO_TAG_BITS)-1)

#ifdef NEKO_JIT_ENABLE

#define PARAMETER_TABLE
#include "opcodes.h"

extern field id_add, id_radd, id_sub, id_rsub, id_mult, id_rmult, id_div, id_rdiv, id_mod, id_rmod;
extern field id_get, id_set;

extern int neko_stack_expand( int_val *sp, int_val *csp, neko_vm *vm );
extern value neko_append_int( neko_vm *vm, value str, int x, bool way );
extern value neko_append_strings( value s1, value s2 );
extern value neko_alloc_module_function( void *m, int_val pos, int nargs );
extern void neko_process_trap( neko_vm *vm );
extern void neko_setup_trap( neko_vm *vm );
extern value NEKO_TYPEOF[];

typedef union {
	void *p;
	unsigned char *b;
	unsigned int *w;
	char *c;
	int *i;
} jit_buffer;

typedef struct _jlist {
	int pos;
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
	int debug_wait;
	jlist *jumps;
	jlist *traps;
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
	OP_MOD,
	OP_LAST,

	OP_GET,
	OP_SET
};

enum IOperation {
	IOP_SHL,
	IOP_SHR,
	IOP_USHR,
	IOP_AND,
	IOP_OR,
	IOP_XOR,
	IOP_LAST
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
#define JOverflow	0x80
#define JCarry		0x82

#define ERROR	{ tmp_free(ctx->pos); tmp_free(ctx->baseptr); failure("JIT error"); }
#define CONST(v)				((int)(int_val)(v))

#define PATCH_JUMP(local)		if( local != NULL ) { \
		int delta = (int)((int_val)buf.p - ((int_val)local + 1)); \
		if( sizeof(*local) == sizeof(int) ) \
			*local = delta - 3; \
		else { \
			if( delta > 127 || delta < -127 ) \
				ERROR; \
			*local = (char)delta; \
		} \
	} \

#define FIELD(n)				((n) * 4)
#define VMFIELD(f)				((int)(int_val)&((neko_vm*)0)->f)
#define FUNFIELD(f)				((int)(int_val)&((vfunction*)0)->f)

#define POS()					((int)((int_val)ctx->buf.p - (int_val)ctx->baseptr))
#define GET_PC()				CONST(ctx->module->code + ctx->curpc)

#define INIT_BUFFER				register jit_buffer buf = ctx->buf
#define END_BUFFER				ctx->buf = buf

#define MOD_RM(mod,reg,rm)		B((mod << 6) | (reg << 3) | rm)
#define SIB						MOD_RM
#define IS_SBYTE(c)				( (c) >= -128 && (c) < 128 )

#define OP_RM(op,mod,reg,rm)	{ B(op); MOD_RM(mod,reg,rm); }
#define OP_ADDR(op,addr,reg,rm) { B(op); \
								MOD_RM(((addr) == 0 && reg != Ebp)?0:(IS_SBYTE(addr)?1:2),rm,reg); \
								if( reg == Esp ) B(0x24); \
								if( (addr) == 0 && reg != Ebp ) {} \
								else if IS_SBYTE(addr) B(addr); \
								else W(addr); }

// OPCODES :
// _r : register
// _c : constant
// _b : 8-bit constant
// _a : [constant]
// _i : [reg]
// _p : [reg + constant:idx]
// _x : [reg + reg:idx * mult]

#define XRet()					B(0xC3)
#define XMov_rr(dst,src)		OP_RM(0x8B,3,dst,src)
#define XMov_rc(dst,cst)		B(0xB8+(dst)); W(cst)
#define XMov_rp(dst,reg,idx)	OP_ADDR(0x8B,idx,reg,dst)
#define XMov_ra(dst,addr)		OP_RM(0x8B,0,dst,5); W(addr)
#define XMov_rx(dst,r,idx,mult) OP_RM(0x8B,0,dst,4); SIB(Mult##mult,idx,r)
#define XMov_pr(dst,idx,src)	OP_ADDR(0x89,idx,dst,src)
#define XMov_pc(dst,idx,c)		OP_ADDR(0xC7,idx,dst,0); W(c)
#define XMov_ar(addr,reg)		B(0x3E); if( reg == Eax ) { B(0xA3); } else { OP_RM(0x89,0,reg,5); }; W(addr)
#define XMov_xr(r,idx,mult,src) OP_RM(0x89,0,src,4); SIB(Mult##mult,idx,r)
#define XCall_r(r)				OP_RM(0xFF,3,2,r)

#define XCall_m_debug(v)		{ \
		int *l; \
		XMov_rr(TMP,Ebp); \
		XSub_rr(TMP,Esp); \
		XAnd_rc(TMP,15); \
		XCmp_rc(TMP,0); \
		XJump(JEq,l); \
		XShr_rc(TMP,2); \
		XPush_r(TMP); \
		XPush_c(CONST(__LINE__)); \
		XMov_rc(TMP,CONST(debug_method_call)); \
		XCall_r(TMP); \
		PATCH_JUMP(l); \
		XMov_rc(TMP,CONST(v)); \
		XCall_r(TMP); \
	}

#define XCall_m_real(v)			XMov_rc(TMP,CONST(v)); XCall_r(TMP);

#ifdef STACK_ALIGN_DEBUG
#	define XCall_m				XCall_m_debug
#else
#	define XCall_m				XCall_m_real
#endif

#define XCall_d(delta)			B(0xE8); W(delta)
#define XPush_r(r)				B(0x50+(r))
#define XPush_c(cst)			B(0x68); W(cst)
#define XPush_p(reg,idx)		OP_ADDR(0xFF,idx,reg,6)
#define XAdd_rc(reg,cst)		if IS_SBYTE(cst) { OP_RM(0x83,3,0,reg); B(cst); } else { OP_RM(0x81,3,0,reg); W(cst); }
#define XAdd_rr(dst,src)		OP_RM(0x03,3,dst,src)
#define XSub_rc(reg,cst)		if IS_SBYTE(cst) { OP_RM(0x83,3,5,reg); B(cst); } else { OP_RM(0x81,3,5,reg); W(cst); }
#define XSub_rr(dst,src)		OP_RM(0x2B,3,dst,src)
#define XCmp_rr(r1,r2)			OP_RM(0x3B,3,r1,r2)
#define XCmp_rc(reg,cst)		if( reg == Eax ) { B(0x3D); } else { OP_RM(0x81,3,7,reg); }; W(cst)
#define XCmp_rb(reg,byte)		OP_RM(0x83,3,7,reg); B(byte)
#define XJump(how,local)		if( (how) == JAlways ) { B(0xE9); } else { B(0x0F); B(how); }; local = buf.i; W(0)
#define XJump_near(local)		B(0xEB); local = buf.c; B(0)
#define XJump_r(reg)			OP_RM(0xFF,3,4,reg)
#define XPop_r(reg)				B(0x58 + (reg))

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
#define XShr_rr(r,src)			if( src != Ecx ) ERROR; shift_r(r,7)
#define XShr_rc(r,n)			shift_c(r,n,7)
#define XUShr_rr(r,src)			if( src != Ecx ) ERROR; shift_r(r,5)
#define XUShr_rc(r,n)			shift_c(r,n,5)

#define XIMul_rr(dst,src)		B(0x0F); B(0xAF); MOD_RM(3,dst,src)
#define XIDiv_r(r)				B(0xF7); MOD_RM(3,7,r)
#define XCdq()					B(0x99);

// FPU
#define XFAddp()				B(0xDE); B(0xC1)
#define XFSubp()				B(0xDE); B(0xE9)
#define XFMulp()				B(0xDE); B(0xC9)
#define XFDivp()				B(0xDE); B(0xF9)
#define XFStp_i(r)				B(0xDD); MOD_RM(0,3,r); if( r == Esp ) B(0x24)
#define XFLd_i(r)				B(0xDD); MOD_RM(0,0,r); if( r == Esp ) B(0x24)
#define XFILd_i(r)				B(0xDB); MOD_RM(0,0,r); if( r == Esp ) B(0x24)

#define is_int(r,flag,local)	{ XTest_rc(r,1); XJump((flag)?JNeq:JEq,local); }

#ifdef STACK_ALIGN
#	define stack_pad(n)				stack_push(Esp,n)
#	define stack_pop_pad(n,n2)		stack_pop(Esp,((n) + (n2)))
#else
#	define stack_pad(n)
#	define stack_pop_pad(n,n2)		stack_pop(Esp,(n))
#endif

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

#define begin_call()	{ XMov_pr(VM,VMFIELD(sp),SP); XMov_pr(VM,VMFIELD(csp),CSP); }
#define end_call()		{ XMov_rp(SP,VM,VMFIELD(sp)); XMov_rp(CSP,VM,VMFIELD(csp)); }
#define label(code)		{ XMov_rc(TMP2,CONST(code)); XCall_r(TMP2); }

#define todo(str)		{ int *loop; XMov_rc(TMP2,CONST(str)); XJump(JAlways,loop); *loop = -5; }

#define pop(n) if( (n) != 0 ) { \
		int i = (n); \
		while( i-- > 0 ) { \
			XMov_pc(SP,FIELD(i),0); \
		} \
		stack_pop(SP,n); \
	}

#define pop_loop(n) { \
		char *start; \
		int *loop; \
		XMov_rc(TMP,n); \
		start = buf.c; \
		XMov_pc(SP,FIELD(0),0); \
		stack_pop(SP,1); \
		XSub_rc(TMP,1); \
		XCmp_rc(TMP,0); \
		XJump(JNeq,loop); \
		*loop = (int)(start - buf.c); \
	}

#ifdef STACK_ALIGN
#	define PAD_OPT(x)	(x)
#else
#	define PAD_OPT(x)	0
#endif

#define runtime_error(msg_id,in_label) { \
	if( in_label ) { stack_pad(2); } else { stack_pad(1); } \
	XPush_c(CONST(strings[msg_id])); \
	if( in_label ) { \
		XMov_rp(TMP2,Esp,FIELD(2+PAD_OPT(2))); \
		XPush_r(TMP2); \
	} else { \
		XPush_c(GET_PC()); \
	} \
	label(code->runtime_error); \
}

#define get_var_r(reg,v) { \
	switch( v ) { \
	case VThis: \
		XMov_rp(reg,VM,VMFIELD(vthis)); \
		break; \
	case VEnv: \
		XMov_rp(reg,VM,VMFIELD(env)); \
		break; \
	case VModule: \
		XMov_rp(reg,VM,VMFIELD(jit_val)); \
		break; \
	case VVm: \
		XMov_rr(reg,VM); \
		break; \
	case VSpMax: \
		XMov_rp(reg,VM,VMFIELD(spmax)); \
		break; \
	case VTrap: \
		XMov_rp(reg,VM,VMFIELD(trap)); \
		break; \
	default: \
		ERROR; \
		break; \
	} \
}

#define get_var_p(reg,idx,v) { \
	switch( v ) { \
	case VThis: \
		XMov_rp(TMP,VM,VMFIELD(vthis)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VEnv: \
		XMov_rp(TMP,VM,VMFIELD(env)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VModule: \
		XMov_rp(TMP,VM,VMFIELD(jit_val)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VVm: \
		XMov_pr(reg,idx,VM); \
		break; \
	case VSpMax: \
		XMov_rp(TMP,VM,VMFIELD(spmax)); \
		XMov_pr(reg,idx,TMP); \
		break; \
	case VTrap: \
		XMov_rp(TMP,VM,VMFIELD(trap)); \
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
		XMov_pr(VM,VMFIELD(vthis),reg); \
		break; \
	case VEnv: \
		XMov_pr(VM,VMFIELD(env),reg); \
		break; \
	case VTrap: \
		XMov_pr(VM,VMFIELD(trap),reg); \
		break; \
	case VModule: \
		XMov_pr(VM,VMFIELD(jit_val),reg); \
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
		XMov_pr(VM,VMFIELD(vthis),TMP); \
		break; \
	case VEnv: \
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,VMFIELD(env),TMP); \
		break; \
	case VTrap: \
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,VMFIELD(trap),TMP); \
		break; \
	case VModule: \
		XMov_rp(TMP,reg,idx); \
		XMov_pr(VM,VMFIELD(jit_val),TMP); \
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
	j->pos = (int)((int_val)jcode - (int_val)ctx->baseptr); \
}

#define setup_before_call(mode,is_callb) { \
	push_infos(is_callb?CALLBACK:PC_ARG); \
	if( !is_callb ) { XPush_r(ACC); } \
	if( mode == THIS_CALL ) { \
		set_var_p(VThis,SP,FIELD(0)); \
		pop(1); \
	} \
	set_var_p(VEnv,ACC,FUNFIELD(env)); \
}

#define restore_after_call(nargs,pad) { \
	int *jok; \
	XCmp_rc(ACC,0); \
	XJump(JNeq,jok); \
	XMov_rp(ACC,Esp,FIELD(nargs+PAD_OPT(pad))); \
	XMov_rp(ACC,ACC,FUNFIELD(module)); \
	stack_pad(-1); \
	XPush_r(ACC); \
	XCall_m(val_throw); \
	PATCH_JUMP(jok); \
	stack_pop_pad(1+nargs,pad); \
	pop_infos(); \
}

#define NARGS (CALL_MAX_ARGS + 1)
#define MAX_ENV		8

typedef struct {
	char *boot;
	char *stack_expand;
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
	char *make_env_n;
	char *oo_get;
	char *oo_set;
	char *handle_trap;
	char *invalid_access;
} jit_code;

char *jit_boot_seq = NULL;
char *jit_handle_trap = NULL;
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
	"Invalid operation (%)", // 7
	"$apply", // 8
	"Invalid End Trap", // 9
	"$hash", // 10
};

#define DEFINE_PROC(p,arg) ctx->buf = buf; jit_##p(ctx,arg); buf = ctx->buf
#define push_infos(arg) DEFINE_PROC(push_infos,arg)
#define test(arg)		DEFINE_PROC(test,arg)
#define call(mode,nargs) ctx->buf = buf; jit_call(ctx,mode,nargs); buf = ctx->buf
#define number_op(arg)	DEFINE_PROC(number_op,arg)
#define array_access(p)	DEFINE_PROC(array_access,p)
#define int_op(arg)		DEFINE_PROC(int_op,arg)
#define best_int()		DEFINE_PROC(best_int,0)

#ifdef STACK_ALIGN_DEBUG
#include <stdlib.h>

static void debug_method_call( int line, int stack ) {
	printf("Stack align error line %d (%d)\n" , line ,stack);
	exit(-1);
}
#endif

#ifdef NEKO_JIT_DEBUG

static void val_print_2( value v ) {
	val_print(alloc_string(" "));
	val_print(v);
}

static void val_print_3( value v ) {
	val_print_2(v);
	val_print(alloc_string("\n"));
}

#endif

static jit_ctx *jit_init_context( void *ptr, int size ) {
	jit_ctx *c = (jit_ctx*)alloc(sizeof(jit_ctx));
	c->size = size;
	c->baseptr = ptr;
	c->buf.p = ptr;
	c->pos = NULL;
	c->curpc = 0;
	c->debug_wait = 0;
	c->jumps = NULL;
	c->traps = NULL;
	return c;
}

static void jit_finalize_context( jit_ctx *ctx ) {
	jlist *l;
	int nbytes = POS();
	if( nbytes == 0 || nbytes > ctx->size )
		*(int*)0xAABBCC = 0;
	l = ctx->jumps;
	while( l != NULL ) {
		*(int*)((char*)ctx->baseptr + l->pos) = ctx->pos[l->target] - (l->pos + 4);
		l = l->next;
	}
	l = ctx->traps;
	while( l != NULL ) {
		*(int*)((char*)ctx->baseptr + l->pos) = ctx->pos[l->target] + (int)(int_val)ctx->baseptr;
		l = l->next;
	}
}

static void jit_push_infos( jit_ctx *ctx, enum PushInfosMode callb ) {
	INIT_BUFFER;
	stack_push(CSP,4);
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

static void jit_best_int( jit_ctx *ctx, int _ ) {
	int *wrap;
	char *jend;
	INIT_BUFFER;
	XMov_rr(TMP,ACC);
	XShl_rc(ACC,1);
	XJump(JOverflow,wrap);
	XOr_rc(ACC,1);
	XJump_near(jend);
	PATCH_JUMP(wrap);
	XPush_r(TMP);
	XCall_m(alloc_int32);
	stack_pop(Esp,1);
	PATCH_JUMP(jend);
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
#	ifdef STACK_ALIGN_DEBUG
	XMov_rr(Ebp,Esp); // ALIGNED STACK
#	endif
	XPush_r(Edi);
	XMov_rp(VM,Esp,FIELD(5));
	get_var_r(TMP,VModule);
	XPush_r(TMP);
	set_var_p(VModule,Esp,FIELD(9));
	XMov_rp(TMP,Esp,FIELD(7));
	XMov_rp(ACC,Esp,FIELD(8));
	end_call();
	XCall_r(TMP);
	begin_call();
	XPop_r(TMP);
	set_var_r(VModule,TMP);
	XPop_r(Edi);
	XPop_r(Esi);
	XPop_r(Ebx);
	XPop_r(Ebp);
	XRet();
	END_BUFFER;
}

static void jit_trap( jit_ctx *ctx, int n ) {
	INIT_BUFFER;

	XMov_rp(VM,Esp,FIELD(1));
	get_var_r(Ebp,VThis);

	// restore vm
	stack_pad(3);
	XPush_r(VM);
	XCall_m(neko_process_trap);
	stack_pop_pad(1,3);

	// restore registers
	end_call();
	XMov_rr(ACC,Ebp);
	XMov_rp(Ebp,VM,VMFIELD(start)+FIELD(1));
	XMov_rp(Esp,VM,VMFIELD(start)+FIELD(2));
	XMov_rp(TMP2,VM,VMFIELD(start)+FIELD(3));

	// restore vm jmp_buf
	XPop_r(TMP);
	XMov_pr(VM,VMFIELD(start)+FIELD(3),TMP);
	XPop_r(TMP);
	XMov_pr(VM,VMFIELD(start)+FIELD(2),TMP);
	XPop_r(TMP);
	XMov_pr(VM,VMFIELD(start)+FIELD(1),TMP);
	XPop_r(TMP);
	XMov_pr(VM,VMFIELD(start),TMP);

	XPush_r(TMP2);
	XRet();

	END_BUFFER;
}

static void jit_stack_expand( jit_ctx *ctx, int _ ) {
	int *jresize, *jdone;
	int max = MAX_STACK_PER_FUNCTION;
	INIT_BUFFER;
	stack_push(CSP,max);
	XCmp_rr(SP,CSP);
	XJump(JLt,jresize);
	stack_pop(CSP,max);
	XRet();
	PATCH_JUMP(jresize);
	stack_pop(CSP,max);
	XPush_r(ACC);
	XPush_r(VM);
	XPush_r(CSP);
	XPush_r(SP);
	XCall_m(neko_stack_expand);
	XCmp_rb(ACC,0);
	XJump(JNeq,jdone);
	stack_pad(-1);
	XPush_c(CONST(strings[0])); // Stack overflow
	XCall_m(val_throw);
	PATCH_JUMP(jdone);
	XMov_rp(ACC,Esp,FIELD(3));
	end_call();
	stack_pop(Esp,4);
	XRet();
	END_BUFFER;
}

static void jit_runtime_error( jit_ctx *ctx, void *unused ) {
	INIT_BUFFER;
	push_infos(PC_ARG); // pc
	begin_call();
	XMov_rp(TMP,Esp,FIELD(2)); // msg on stack
	XPush_r(TMP);
	XCall_m(val_throw);
	END_BUFFER;
}

static void jit_invalid_access( jit_ctx *ctx, int _ ) {
	INIT_BUFFER;
	int *jnext;

	// if( val_field_name(f) == val_null ) RuntimeError("Invalid field access")
	stack_pad(1);
	XMov_rp(TMP,Esp,FIELD(2)); // field
	XPush_r(TMP);
	XCall_m(val_field_name);
	stack_pop_pad(1,1);
	XCmp_rc(ACC,CONST(val_null));
	XJump(JNeq,jnext);
	runtime_error(5,true);

	// else {
	//    b = alloc_buffer("Invalid field access : ");
	PATCH_JUMP(jnext);
	XPush_r(ACC);
	XPush_c(CONST("Invalid field access : "));
	XCall_m(alloc_buffer);
	stack_pop(Esp,1);
	//    val_buffer(b,v);
	XPush_r(ACC);
	XCall_m(val_buffer);
	//    buffer_to_string(b);
	XCall_m(buffer_to_string);
	stack_pop(Esp,2);
	push_infos(PC_ARG); // pc
	begin_call();
	XPush_r(ACC);
	XCall_m(val_throw);
	END_BUFFER;
}

static void jit_test( jit_ctx *ctx, int how ) {
	INIT_BUFFER;
	int *jnot1, *jnot2;
	char *jend;
	// call val_compare(sp[0],acc)
	XPush_r(ACC);
	XMov_rp(TMP,SP,FIELD(0));
	XPush_r(TMP);
	begin_call();
	XCall_m(val_compare);
	end_call();
	stack_pop(Esp,2);
	pop(1);
	// test ok and != invalid_comparison
	XCmp_rc(ACC,0);
	XJump(how,jnot1);
	XCmp_rc(ACC,invalid_comparison);
	XJump(JEq,jnot2);
	XMov_rc(ACC,CONST(val_true));
	XJump_near(jend);
	PATCH_JUMP(jnot1);
	PATCH_JUMP(jnot2);
	XMov_rc(ACC,CONST(val_false));
	PATCH_JUMP(jend);
	END_BUFFER;
}

static void jit_call( jit_ctx *ctx, int mode, int nargs ) {
	INIT_BUFFER;
	int *jerr, *jother, *jerr2;
	char *jend1, *jend2, *jend3;

// if( is_int ) : error
	is_int(ACC,1,jerr);

// if( type == jit )
	XMov_rp(TMP,ACC,FUNFIELD(t));
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
		XJump_near(jend1);
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

	XJump_near(jend2);

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

	XJump_near(jend3);

// else error
	PATCH_JUMP(jerr);
	PATCH_JUMP(jerr2);
	runtime_error(3,false); // Invalid call

// end
	PATCH_JUMP(jend1);
	PATCH_JUMP(jend2);
	PATCH_JUMP(jend3);
	stack_pop(Esp,1); // pushed pc

	END_BUFFER;
}

static void jit_call_jit( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	int *jerr;

	// check arg count
	XMov_rp(TMP,ACC,FUNFIELD(nargs));
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jerr);

	if( mode == TAIL_CALL ) {
		// pop PC and EIP from the stack
		stack_pop(Esp,2);

		set_var_p(VModule,ACC,FUNFIELD(module));
		set_var_p(VEnv,ACC,FUNFIELD(env));
		XMov_rp(TMP,ACC,FUNFIELD(addr));
		XJump_r(TMP);
	} else {
		push_infos(PC_ARG);
		set_var_p(VModule,ACC,FUNFIELD(module));
		set_var_p(VEnv,ACC,FUNFIELD(env));
		if( mode == THIS_CALL ) {
			set_var_p(VThis,SP,FIELD(0));
			pop(1);
		}
		XMov_rp(TMP,ACC,FUNFIELD(addr));
		stack_pad(1);
		XCall_r(TMP);
		stack_pad(-1);
		pop_infos();
		XRet();
	}
	PATCH_JUMP(jerr);
	runtime_error(3,true); // Invalid call
	END_BUFFER;
}

static void jit_call_prim( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	int *jvararg, *jerr;
	int i;
#	ifdef STACK_ALIGN
	int pad_size = 4 - ((2+nargs)%4);
#	endif

	// check arg count
	XMov_rp(TMP,ACC,FUNFIELD(nargs));
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jvararg);

	// push args from VMSP to PROCSP
	setup_before_call(mode,false);
	stack_pad(pad_size);
	for(i=0;i<nargs;i++) {
		XPush_p(SP,FIELD(i));
	}
#	ifndef NEKO_JIT_DEBUG
	pop(nargs);
#	endif

	// call C primitive
	XMov_rp(TMP,ACC,FUNFIELD(addr));
	begin_call();
	XCall_r(TMP);
	end_call();
	restore_after_call(nargs,pad_size);

#	ifdef NEKO_JIT_DEBUG
	pop(nargs);
#	endif
	XRet();

//	else if( args == VAR_ARGS )
	PATCH_JUMP(jvararg);
	XCmp_rb(TMP,VAR_ARGS);
	XJump(JNeq,jerr);

	// push args from VMSP to PROCSP
	setup_before_call(mode,false);
	stack_pad(3);
	for(i=0;i<nargs;i++) {
		XPush_p(SP,FIELD(i));
	}
	pop(nargs);

	// push arg ptr and arg count
	XMov_rr(TMP,Esp);
	XPush_c(nargs);
	XPush_r(TMP);

	// call C primitive
	XMov_rp(TMP,ACC,FUNFIELD(addr));
	begin_call();

	XCall_r(TMP);
	end_call();
	restore_after_call(2 + nargs,3);
	XRet();

// error
	PATCH_JUMP(jerr);
	runtime_error(3,true); // Invalid call
	END_BUFFER;
}

static void jit_call_fun( jit_ctx *ctx, int nargs, int mode ) {
	INIT_BUFFER;
	int *jerr;

	// check arg count
	XMov_rp(TMP,ACC,FUNFIELD(nargs));
	XCmp_rb(TMP,nargs);
	XJump(JNeq,jerr);

	// C call : neko_interp(vm,m,acc,pc)
	setup_before_call(mode,true);
	stack_push(Esp,4);
	XMov_rp(TMP,ACC,FUNFIELD(addr));
	XMov_pr(Esp,FIELD(3),TMP);
	XMov_pr(Esp,FIELD(2),ACC);
	get_var_p(Esp,FIELD(1),VModule);
	get_var_p(Esp,FIELD(0),VVm);
	begin_call();
	XCall_m(neko_interp);
	end_call();
	stack_pop(Esp,4);
	XRet();

	PATCH_JUMP(jerr);
	runtime_error(3,true); // Invalid call
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

// ------------------- INTERP EMULATION

#define STACK_EXPAND if( !neko_stack_expand(vm->sp,vm->csp,vm) ) val_throw(alloc_string("Stack Overflow"))

#define ERASE 0

#define ACC_BACKUP
#define ACC_RESTORE

#define PushInfos() \
		if( vm->csp + 4 >= vm->sp ) STACK_EXPAND; \
		*++vm->csp = (int_val)pc; \
		*++vm->csp = (int_val)vm->env; \
		*++vm->csp = (int_val)vm->vthis; \
		*++vm->csp = (int_val)vm->jit_val;

#define PopInfos(restpc) \
		vm->jit_val = (void*)*vm->csp; \
		*vm->csp-- = ERASE; \
		vm->vthis = (value)*vm->csp; \
		*vm->csp-- = ERASE; \
		vm->env = (value)*vm->csp; \
		*vm->csp-- = ERASE; \
		if( restpc ) pc = (int)*vm->csp; \
		*vm->csp-- = ERASE;

#define BeginCall()
#define EndCall()

#define RuntimeError(err)	{ PushInfos(); BeginCall(); val_throw(alloc_string(err)); }

#define ObjectOpGen(obj,param,id,err) { \
		value _o = (value)obj; \
		value _arg = (value)param; \
		value _f = val_field(_o,id); \
		if( _f == val_null ) { \
			err; \
		} else { \
			PushInfos(); \
			BeginCall(); \
			acc = (int_val)val_callEx(_o,_f,&_arg,1,NULL); \
			EndCall(); \
			PopInfos(false); \
		} \
	}

#define ObjectOp(obj,param,id) ObjectOpGen(obj,param,id,RuntimeError("Unsupported operation"))

#define OpError(op) RuntimeError("Invalid operation (" op ")")

static int_val generic_add( neko_vm *vm, int_val acc, int_val sp, int pc ) {
	if( acc & 1 ) {
		if( val_tag(sp) == VAL_FLOAT )
			acc = (int_val)alloc_float(val_float(sp) + val_int(acc));
		else if( val_tag(sp) == VAL_INT32 )
			acc = (int_val)alloc_best_int(val_int32(sp) + val_int(acc));
		else if( val_short_tag(sp) == VAL_STRING  )
			acc = (int_val)neko_append_int(vm,(value)sp,val_int(acc),true);
		else if( val_tag(sp) == VAL_OBJECT )
			ObjectOp(sp,acc,id_add)
		else
			OpError("+");
	} else if( sp & 1 ) {
		if( val_tag(acc) == VAL_FLOAT )
			acc = (int_val)alloc_float(val_int(sp) + val_float(acc));
		else if( val_tag(acc) == VAL_INT32 )
			acc = (int_val)alloc_best_int(val_int(sp) + val_int32(acc));
		else if( val_short_tag(acc) == VAL_STRING )
			acc = (int_val)neko_append_int(vm,(value)acc,val_int(sp),false);
		else if( val_tag(acc) == VAL_OBJECT )
			ObjectOp(acc,sp,id_radd)
		else
			OpError("+");
	} else if( val_tag(acc) == VAL_FLOAT ) {
		if( val_tag(sp) == VAL_FLOAT )
			acc = (int_val)alloc_float(val_float(sp) + val_float(acc));
		else if( val_tag(sp) == VAL_INT32 )
			acc = (int_val)alloc_float(val_int32(sp) + val_float(acc));
		else
			goto add_next;
	} else if( val_tag(acc) == VAL_INT32 ) {
		if( val_tag(sp) == VAL_INT32 )
			acc = (int_val)alloc_best_int(val_int32(sp) + val_int32(acc));
		else if( val_tag(sp) == VAL_FLOAT )
			acc = (int_val)alloc_float(val_float(sp) + val_int32(acc));
		else
			goto add_next;
	} else {
	add_next:
		if( val_tag(sp) == VAL_OBJECT )
			ObjectOpGen(sp,acc,id_add,goto add_2)
		else {
			add_2:
			if( val_tag(acc) == VAL_OBJECT )
				ObjectOpGen(acc,sp,id_radd,goto add_3)
			else {
				add_3:
				if( val_short_tag(acc) == VAL_STRING || val_short_tag(sp) == VAL_STRING ) {
					ACC_BACKUP
					buffer b = alloc_buffer(NULL);
					BeginCall();
					val_buffer(b,(value)sp);
					ACC_RESTORE;
					val_buffer(b,(value)acc);
					EndCall();
					acc = (int_val)buffer_to_string(b);
				} else
					OpError("+");
			}
		}
	}
	return acc;
}

#define NumberOp(op,fop,id_op,id_rop) \
		if( acc & 1 ) { \
			if( val_tag(sp) == VAL_FLOAT ) \
				acc = (int_val)alloc_float(fop(val_float(sp),val_int(acc))); \
			else if( val_tag(sp) == VAL_INT32 ) \
				acc = (int_val)alloc_best_int(val_int32(sp) op val_int(acc)); \
			else if( val_tag(sp) == VAL_OBJECT ) \
			    ObjectOp(sp,acc,id_op) \
			else \
				OpError(#op); \
		} else if( sp & 1 ) { \
			if( val_tag(acc) == VAL_FLOAT ) \
				acc = (int_val)alloc_float(fop(val_int(sp),val_float(acc))); \
			else if( val_tag(acc) == VAL_INT32 ) \
				acc = (int_val)alloc_best_int(val_int(sp) op val_int32(acc)); \
			else if( val_tag(acc) == VAL_OBJECT ) \
				ObjectOp(acc,sp,id_rop) \
			else \
				OpError(#op); \
		} else if( val_tag(acc) == VAL_FLOAT ) { \
			if( val_tag(sp) == VAL_FLOAT ) \
				acc = (int_val)alloc_float(fop(val_float(sp),val_float(acc))); \
			else if( val_tag(sp) == VAL_INT32 ) \
				acc = (int_val)alloc_float(fop(val_int32(sp),val_float(acc))); \
			else \
				goto id_op##_next; \
		} else if( val_tag(acc) == VAL_INT32 ) {\
			if( val_tag(sp) == VAL_INT32 ) \
				acc = (int_val)alloc_best_int(val_int32(sp) op val_int32(acc)); \
			else if( val_tag(sp) == VAL_FLOAT ) \
				acc = (int_val)alloc_float(fop(val_float(sp),val_int32(acc))); \
			else \
				goto id_op##_next; \
		} else { \
			id_op##_next: \
			if( val_tag(sp) == VAL_OBJECT ) \
				ObjectOpGen(sp,acc,id_op,goto id_op##_next2) \
			else { \
				id_op##_next2: \
				if( val_tag(acc) == VAL_OBJECT ) \
					ObjectOp(acc,sp,id_rop) \
				else \
					OpError(#op); \
			} \
		}

#define SUB(x,y) ((x) - (y))
#define MULT(x,y) ((x) * (y))
#define DIV(x,y) ((x) / (y))

#define GENERIC_OP(id,op,fop) \
	static int_val generic_##id( neko_vm *vm, int_val acc, int_val sp, int pc ) { \
		NumberOp(op,fop,id_##id,id_r##id); \
		return acc; \
	}

GENERIC_OP(sub,-,SUB);
GENERIC_OP(mult,*,MULT);

static int_val generic_div( neko_vm *vm, int_val acc, int_val sp, int pc ) {
	if( val_is_number(acc) && val_is_number(sp) )
		acc = (int_val)alloc_float( ((tfloat)val_number(sp)) / val_number(acc) );
	else if( val_is_object(sp) )
		ObjectOpGen(sp,acc,id_div,goto div_next)
	else {
		div_next:
		if( val_is_object(acc) )
			ObjectOp(acc,sp,id_rdiv)
		else
			OpError("/");
	}
	return acc;
}

static int_val generic_mod( neko_vm *vm, int_val acc, int_val sp, int pc ) {
	if( (acc == 1 || (val_is_int32(acc) && val_int32(acc)==0)) && val_is_any_int(sp) )
		OpError("%");
	NumberOp(%,fmod,id_mod,id_rmod);
	return acc;
}

#define GENERIC_IOP(id,op) \
	static int_val generic_##id( neko_vm *vm, int_val acc, int_val sp, int pc ) { \
		if( val_is_any_int(acc) && val_is_any_int(sp) ) \
			acc = (int_val)alloc_best_int(val_any_int(sp) op val_any_int(acc)); \
		else \
			OpError(#op); \
		return acc; \
	}

GENERIC_IOP(shl,<<);
GENERIC_IOP(shr,>>);
GENERIC_IOP(or,|);
GENERIC_IOP(and,&);
GENERIC_IOP(xor,^);

static int_val generic_ushr( neko_vm *vm, int_val acc, int_val sp, int pc ) {
	if( val_is_any_int(acc) && val_is_any_int(sp) )
		acc = (int_val)alloc_best_int( ((unsigned int)val_any_int(sp)) >> val_any_int(acc));
	else
		OpError(">>>");
	return acc;
}

// --------------------------------------------

// we only inline operations for (int,int) and (float,float)
// other cases are handled by a single generic_op primitive
// through a small jit_generic_* wrapper

static void jit_number_op( jit_ctx *ctx, enum Operation op ) {
	INIT_BUFFER;
	int *jnot_int, *jnot_int2, *jint, *jnot_float1, *jnot_float2, *jmod0;
	char *jend, *jend2, *jdiv = NULL;
	// tmp = acc
	XMov_rr(TMP,ACC);
	// acc = *sp
	XMov_rp(ACC,SP,FIELD(0));
	// is_int(acc) && is_int(sp)
	is_int(ACC,false,jnot_int);
	is_int(TMP,false,jnot_int2);
	XShr_rc(ACC,1);
	XShr_rc(TMP,1);
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
	case OP_MOD:
		XCmp_rc(TMP,0);
		XJump(JNeq,jmod0);
		runtime_error(7,false);
		PATCH_JUMP(jmod0);
		XCdq();
		XIDiv_r(TMP);
		XMov_rr(ACC,Edx);
		break;
	case OP_DIV:
		XPush_r(ACC);
		XFILd_i(Esp);
		XPush_r(TMP);
		XFILd_i(Esp);
		stack_pop(Esp,2);
		XJump_near(jdiv);
		break;
	default:
		ERROR;
		break;
	}
	best_int();
	XJump_near(jend);

	// is_float(acc) && is_float(sp)
	PATCH_JUMP(jnot_int);
	XMov_rp(TMP2,ACC,FIELD(0));
	XCmp_rb(TMP2,VAL_FLOAT);
	XJump(JNeq,jnot_float1);
	is_int(TMP,true,jint);
	XMov_rp(TMP2,TMP,FIELD(0));
	XCmp_rb(TMP2,VAL_FLOAT);
	XJump(JNeq,jnot_float2);
	
	// load floats
	XAdd_rc(ACC,4);
	XFLd_i(ACC);
	XAdd_rc(TMP,4);
	XFLd_i(TMP);

	switch( op ) {
	case OP_ADD:
		XFAddp();
		break;
	case OP_SUB:
		XFSubp();
		break;
	case OP_DIV:
		PATCH_JUMP(jdiv);
		XFDivp();
		break;
	case OP_MUL:
		XFMulp();
		break;
	case OP_MOD:
		stack_push(Esp,2);
		XFStp_i(Esp);
		stack_push(Esp,2);
		XFStp_i(Esp);
		XCall_m(fmod);
		stack_pop(Esp,2);
		break;
	default:
		ERROR;
		break;
	}
	if( op != OP_MOD ) {
		stack_push(Esp,2);
	}	
	XFStp_i(Esp);
	XCall_m(alloc_float);
	stack_pop(Esp,2);
	XJump_near(jend2);

	// else...
	PATCH_JUMP(jint);
	PATCH_JUMP(jnot_float1);
	PATCH_JUMP(jnot_float2);
	PATCH_JUMP(jnot_int2);

	begin_call();
	XPush_c(GET_PC());	
	XPush_r(ACC);
	XPush_r(TMP);
	XPush_r(VM);
	switch( op ) {
	case OP_ADD:
		XCall_m(generic_add);
		break;
	case OP_SUB:
		XCall_m(generic_sub);
		break;
	case OP_DIV:
		XCall_m(generic_div);
		break;
	case OP_MUL:
		XCall_m(generic_mult);
		break;
	case OP_MOD:
		XCall_m(generic_mod);
		break;
	case OP_GET:
	case OP_SET:
	case OP_LAST:
		// not used here
		break;
	}
	stack_pop(Esp,4);
	end_call();

	PATCH_JUMP(jend);
	PATCH_JUMP(jend2);
	pop(1);
	END_BUFFER;
}

static void jit_int_op( jit_ctx *ctx, enum IOperation op ) {
	INIT_BUFFER;
	int *jerr1, *jerr2;
	char *jend;

	XMov_rr(TMP,ACC);
	XMov_rp(ACC,SP,FIELD(0));
	is_int(ACC,false,jerr1);
	is_int(TMP,false,jerr2);
	XShr_rc(TMP,1);
	XShr_rc(ACC,1);

	switch( op ) {
	case IOP_SHL:
		XShl_rr(ACC,TMP);
		break;
	case IOP_SHR:
		XShr_rr(ACC,TMP);
		break;
	case IOP_USHR:
		XUShr_rr(ACC,TMP);
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

	best_int();
	XJump_near(jend);

	PATCH_JUMP(jerr1);
	PATCH_JUMP(jerr2);

	begin_call();
	XPush_c(GET_PC());
	XPush_r(ACC); // acc and tmp are reversed in jit_number_op
	XPush_r(TMP); //
	XPush_r(VM);
	switch( op ) {
	case IOP_SHL:
		XCall_m(generic_shl);
		break;
	case IOP_SHR:
		XCall_m(generic_shr);
		break;
	case IOP_USHR:
		XCall_m(generic_ushr);
		break;
	case IOP_AND:
		XCall_m(generic_and);
		break;
	case IOP_OR:
		XCall_m(generic_or);
		break;
	case IOP_XOR:
		XCall_m(generic_xor);
		break;
	case IOP_LAST:
		// nothing
		break;
	}
	stack_pop(Esp,4);
	end_call();
	
	PATCH_JUMP(jend);
	pop(1);

	END_BUFFER;
}

static void jit_array_access( jit_ctx *ctx, int n ) {
	INIT_BUFFER;
	int *jerr1, *jerr2;
	char *jend1, *jend2 = NULL, *jend3;
	int *jnot_array, *jbounds = NULL;

	is_int(ACC,true,jerr1);
	XMov_rp(TMP,ACC,0);
	XMov_rr(TMP2,TMP);
	XAnd_rc(TMP2,TAG_MASK);
	XCmp_rb(TMP2,VAL_ARRAY);

	XJump(JNeq,jnot_array);
	if( n > 0 ) {
		XUShr_rc(TMP,NEKO_TAG_BITS);
		XCmp_rc(TMP,n);
		XJump(JLte,jbounds);
	}
	XMov_rp(ACC,ACC,FIELD(n + 1));
	XJump_near(jend1);

	if( n > 0 ) {
		PATCH_JUMP(jbounds);
		XMov_rc(ACC,CONST(val_null));
		XJump_near(jend2);
	}

	PATCH_JUMP(jnot_array);
	XCmp_rb(TMP2,VAL_OBJECT);
	XJump(JNeq,jerr2);
	XPush_c(GET_PC());
	XMov_rr(TMP,ACC);
	XMov_rc(ACC,CONST(alloc_int(n)));
	label(code->oo_get);
	stack_pop(Esp,1);
	XJump_near(jend3);
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
	int *jerr1, *jerr2, *jok;
	int i;

	if( esize == -1 ) {
		XMov_rr(TMP2,TMP); // store esize
	}

	// check type t_function or t_jit
	is_int(ACC,true,jerr1);
	XMov_rp(TMP,ACC,FUNFIELD(t));
	XCmp_rb(TMP,VAL_JITFUN);
	XJump(JEq,jok);
	XCmp_rb(TMP,VAL_FUNCTION);
	XJump(JNeq,jerr2);
	PATCH_JUMP(jok);

	// prepare args for alloc_module_function
	XPush_r(TMP);				// acc->type
	stack_push(Esp,1);			// empty cell
	stack_pad(2);
	XMov_rp(TMP,ACC,FUNFIELD(nargs));
	XPush_r(TMP);
	XMov_rp(TMP,ACC,FUNFIELD(addr));
	XPush_r(TMP);
	XMov_rp(TMP,ACC,FUNFIELD(module));
	XPush_r(TMP);

	// call alloc_array(n)
	stack_pad(3);
	if( esize == -1 ) {
		XPush_r(TMP2);
	} else {
		XPush_c(esize);
	}
	XCall_m(alloc_array);
	if( esize == -1 ) {
		char *start;
		int *loop;
		XPop_r(TMP2);
		stack_pad(-3);

		// fill array
		start = buf.c;
		XMov_rp(TMP,SP,FIELD(0));
		XMov_xr(ACC,TMP2,4,TMP);
		XMov_pc(SP,FIELD(0),0);
		stack_pop(SP,1);
		XSub_rc(TMP2,1);
		XCmp_rc(TMP2,0);
		XJump(JNeq,loop);
		*loop = (int)(start - buf.c);
	} else {
		stack_pop_pad(1,3);

		// fill array
		for(i=0;i<esize;i++) {
			XMov_rp(TMP,SP,FIELD(i));
			XMov_pr(ACC,FIELD(esize-i),TMP);
		}
		pop(esize);
	}

	// call alloc_module_function
	XMov_pr(Esp,FIELD(3),ACC); // save acc
	XCall_m(neko_alloc_module_function);
	XMov_rp(TMP,Esp,FIELD(3)); // restore acc
	XMov_rp(TMP2,Esp,FIELD(4)); // restore type
	stack_pop_pad(5,2);
	XMov_pr(ACC,FUNFIELD(t),TMP2);
	XMov_pr(ACC,FUNFIELD(env),TMP);
	XRet();

	// errors
	PATCH_JUMP(jerr1);
	PATCH_JUMP(jerr2);
	runtime_error(6,true); // Invalid environment
	END_BUFFER;
}

#define REG_ACC ((op == OP_ADD)?ACC:TMP)
#define REG_TMP	((op == OP_ADD)?TMP:ACC)

static void jit_object_op_gen( jit_ctx *ctx, enum Operation op, int right ) {
	int *next;
	field f;
	int is_opset;

	INIT_BUFFER;

	f = 0;
	is_opset = 0;
	switch( op ) {
	case OP_ADD:
		f = (right ? id_radd : id_add);
		break;
	case OP_SUB:
		f = (right ? id_rsub : id_sub);
		break;
	case OP_MUL:
		f = (right ? id_rmult : id_mult);
		break;
	case OP_DIV:
		f = (right ? id_rdiv : id_div);
		break;
	case OP_MOD:
		f = (right ? id_rmod : id_mod);
		break;
	case OP_GET:
		f = id_get;
		break;
	case OP_SET:
		f = id_set;
		is_opset = 1;
		break;
	default:
		ERROR;
	}

	// prepare args
	XPush_r(right?REG_TMP:REG_ACC);
	if( is_opset ) {
		XMov_rp(TMP2,Esp,FIELD(3));
		XPush_r(TMP2);
	}
	XMov_rr(TMP2,Esp);
	XPush_c(0);
	XPush_c(is_opset?2:1);
	XPush_r(TMP2);
	XPush_r(right?REG_ACC:REG_TMP);

	XPush_c(f);
	XPush_r(right?REG_ACC:REG_TMP);
	XCall_m(val_field);
	stack_pop(Esp,2);
	XCmp_rc(ACC,CONST(val_null));
	XJump(JNeq,next);
	stack_pop(Esp,is_opset?6:5);
	runtime_error(11,true); // Unsupported operation

	PATCH_JUMP(next);
	XPop_r(TMP);

	XPush_r(ACC);
	XPush_r(TMP);
	begin_call();
	XCall_m(val_callEx);
	end_call();
	stack_pop(Esp,is_opset?7:6);
	XRet();
	END_BUFFER;
}

static void jit_object_get( jit_ctx *ctx, int _ ) {
	jit_object_op_gen(ctx,OP_GET,true);
}

static void jit_object_set( jit_ctx *ctx, int _ ) {
	jit_object_op_gen(ctx,OP_SET,true);
}

static void jit_opcode( jit_ctx *ctx, enum OPCODE op, int p ) {
	INIT_BUFFER;
	int i;
	int *jok;
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
	case AccInt32:
		XPush_c(p);
		XCall_m(alloc_int32);
		stack_pop(Esp,1);
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
		XCmp_rc(TMP2,(p << NEKO_TAG_BITS) | VAL_ARRAY);
		XJump(JGt,jok);
		runtime_error(1,false); // Reading Outside Env
		PATCH_JUMP(jok);
		XMov_rp(ACC,TMP,FIELD(p + 1)); // acc = val_array_ptr(env)[p]
		break;
	case AccArray: {
		int *jerr1, *jerr2, *jerr3, *jerr4, *jnot_array, *jbounds;
		char *jend1, *jend2, *jend3, *jend4;

		// check array & int
		XMov_rp(TMP,SP,FIELD(0));
		pop(1);
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XAnd_rc(TMP2,TAG_MASK);
		XCmp_rb(TMP2,VAL_ARRAY);
		XJump(JNeq,jnot_array);
		is_int(ACC,false,jerr2);

		// check bounds & access array
		XShr_rc(ACC,1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XUShr_rc(TMP2,NEKO_TAG_BITS);
		XCmp_rr(ACC,TMP2);
		XJump(JGte,jbounds);
		XAdd_rc(ACC,1);			  // acc = val_array_ptr(tmp)[acc]
		XMov_rx(ACC,TMP,ACC,4);
		XJump_near(jend1);

		// outside bounds
		PATCH_JUMP(jbounds);
		XMov_rc(ACC,CONST(val_null));
		XJump_near(jend2);

		// check object
		PATCH_JUMP(jnot_array);
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr3);
		XPush_c(GET_PC());
		label(code->oo_get);
		stack_pop(Esp,1);
		XJump_near(jend3);

		// check int32 index (with array)
		PATCH_JUMP(jerr2);
		XMov_rp(TMP2,ACC,FIELD(0));
		XCmp_rb(TMP2,VAL_INT32);
		XJump(JNeq,jerr4);
		XMov_rc(ACC,CONST(val_null));
		XJump_near(jend4);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr3);
		PATCH_JUMP(jerr4);
		runtime_error(4,false); // Invalid array access
		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
		PATCH_JUMP(jend4);
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
		int *jerr1, *jerr2, *jend1, *loop, *no_resolver;
		char *jend2, *jend3, *start;

		is_int(ACC,true,jerr1);
		XMov_rp(TMP,ACC,FIELD(0));
		XCmp_rb(TMP,VAL_OBJECT);
		XJump(JNeq,jerr2);
		XPush_r(ACC);
		XPush_r(VM);
		stack_pad(1);
		XMov_rr(VM,ACC);
		XPush_c(p);
		start = buf.c;
		XMov_rr(TMP,VM);
		XAdd_rc(TMP,FIELD(1));
		XPush_r(TMP);
		XCall_m(otable_find);
		XCmp_rc(ACC,0);
		XJump(JNeq,jend1);
		stack_pop(Esp,1);
		XMov_rp(VM,VM,FIELD(3)); // acc = acc->proto
		XCmp_rc(VM,0);
		XJump(JNeq,loop);
		*loop = (int)(start - buf.c);
		stack_pop_pad(1,1);
		XPop_r(VM);
		XPop_r(ACC);
		XMov_rp(TMP,VM,VMFIELD(resolver));
		XCmp_rc(TMP,0);
		XJump(JEq,no_resolver);

        XPush_c(CONST(alloc_int(p)));
		XPush_r(ACC);
		XPush_r(TMP);
		begin_call();
		XCall_m(val_call2);
		end_call();
		stack_pop(Esp,3);
		XJump_near(jend3);

		PATCH_JUMP(no_resolver);
		XMov_rc(ACC,CONST(val_null));
		XJump_near(jend2);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		XPush_c(p);
		XPush_c(GET_PC());
		label(code->invalid_access);

		PATCH_JUMP(jend1);
		stack_pop_pad(2,2);
		XPop_r(VM);
		stack_pop(Esp,1);
		XMov_rp(ACC,ACC,FIELD(0));
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
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
		XCmp_rc(TMP2,(p << NEKO_TAG_BITS) | VAL_ARRAY);
		XJump(JGt,jok);
		runtime_error(2,false); // Writing Outside Env
		PATCH_JUMP(jok);
		XMov_pr(TMP,FIELD(p+1),ACC); // val_array_ptr(env)[p] = acc
		break;
	case SetThis:
		set_var_r(VThis,ACC);
		break;
	case SetField: {
		int *jerr1, *jerr2;
		char *jend;
		XMov_rp(TMP,SP,FIELD(0));
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr2);

		// call otable_replace(table,field,acc)
		stack_pad(2);
		XPush_r(ACC);
		XPush_c(p);
		XAdd_rc(TMP,FIELD(1));
		XPush_r(TMP);
		XCall_m(otable_replace);
		stack_pop(Esp,3);
		XMov_rp(ACC,Esp,FIELD(-1));
		stack_pad(-2);
		XJump_near(jend);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(5,false);
		PATCH_JUMP(jend);
		pop(1);
		break;
		}
	case SetArray: {
		int *jerr1, *jerr2, *jerr3, *jnot_array, *jend1, *jend4;
		char *jend2, *jend3;
		XMov_rp(TMP,SP,FIELD(0)); // sp[0] : array/object
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XAnd_rc(TMP2,TAG_MASK);
		XCmp_rb(TMP2,VAL_ARRAY);
		XJump(JNeq,jnot_array);

		XMov_rp(TMP2,SP,FIELD(1)); // sp[1] : index
		is_int(TMP2,false,jerr2);

		XMov_rp(TMP,TMP,FIELD(0)); // tmp = tmp->type
		XShr_rc(TMP2,1);
		XUShr_rc(TMP,NEKO_TAG_BITS);
		XCmp_rr(TMP2,TMP);
		XJump(JGte,jend1);

		XMov_rp(TMP,SP,FIELD(0));
		XAdd_rc(TMP2,1);
		XMov_xr(TMP,TMP2,4,ACC);
		XJump_near(jend2);

		PATCH_JUMP(jnot_array);
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr3);

		XMov_rp(TMP2,SP,FIELD(1)); // index
		XPush_r(TMP2);
		XPush_c(GET_PC());
		label(code->oo_set);
		stack_pop(Esp,2);
		XJump_near(jend3);

		// check int32 index (with array)
		PATCH_JUMP(jerr2);
		XMov_rp(TMP2,TMP2,FIELD(0));
		XCmp_rb(TMP2,VAL_INT32);
		XJump(JEq,jend4);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr3);
		runtime_error(4,false);

		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
		PATCH_JUMP(jend4);
		pop(2);
		break;
		}
	case SetIndex: {
		int *jerr1, *jerr2, *jnot_array, *jend1;
		char *jend2, *jend3;
		XMov_rp(TMP,SP,FIELD(0)); // sp[0] : array / object
		pop(1);
		is_int(TMP,true,jerr1);
		XMov_rp(TMP2,TMP,FIELD(0));
		XAnd_rc(TMP2,TAG_MASK);
		XCmp_rb(TMP2,VAL_ARRAY);
		XJump(JNeq,jnot_array);

		XMov_rp(TMP2,TMP,FIELD(0));
		XCmp_rc(TMP2,(p << NEKO_TAG_BITS) | VAL_ARRAY); // fake header
		XJump(JLte,jend1);
		XMov_pr(TMP,FIELD(p + 1),ACC);
		XJump_near(jend2);

		PATCH_JUMP(jnot_array);
		XCmp_rb(TMP2,VAL_OBJECT);
		XJump(JNeq,jerr2);
		XPush_c(CONST(alloc_int(p)));
		XPush_c(GET_PC());
		label(code->oo_set);
		stack_pop(Esp,2);
		XJump_near(jend3);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(4,false);

		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		PATCH_JUMP(jend3);
		break;
		}
	case Push:
		stack_push(SP,1);
		XMov_pr(SP,FIELD(0),ACC);
		break;
	case Pop:
		if( p > 10 ) {
			pop_loop(p);
		} else {
			pop(p);
		}
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
		int *jnot1, *jnot2;
		char *jend;
		// call val_compare(sp[0],acc)
		XPush_r(ACC);
		XMov_rp(TMP,SP,FIELD(0));
		XPush_r(TMP);
		begin_call();
		XCall_m(val_compare);
		end_call();
		stack_pop(Esp,2);
		pop(1);
		// test if ok and != invalid_comparison
		XCmp_rc(ACC,0);
		XJump(JNeq,jnot1);
		XCmp_rc(ACC,invalid_comparison);
		XJump(JEq,jnot2);
		XMov_rc(ACC,CONST(val_false));
		XJump_near(jend);
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
		int *jfalse1, *jfalse2, *jfalse3;
		char *jend;
		XCmp_rc(ACC,CONST(val_false));
		XJump(JEq,jfalse1);
		XCmp_rc(ACC,CONST(val_null));
		XJump(JEq,jfalse2);
		XCmp_rc(ACC,CONST(alloc_int(0)));
		XJump(JEq,jfalse3);
		XMov_rc(ACC,CONST((op == Bool)?val_true:val_false));
		XJump_near(jend);
		PATCH_JUMP(jfalse1);
		PATCH_JUMP(jfalse2);
		PATCH_JUMP(jfalse3);
		XMov_rc(ACC,CONST((op == Bool)?val_false:val_true));
		PATCH_JUMP(jend);
		break;
		}
	case IsNull: {
		int *jnext;
		char *jend;
		XCmp_rc(ACC,CONST(val_null));
		XJump(JNeq,jnext);
		XMov_rc(ACC,CONST(val_true));
		XJump_near(jend);
		PATCH_JUMP(jnext);
		XMov_rc(ACC,CONST(val_false));
		PATCH_JUMP(jend);
		break;
		}
	case IsNotNull: {
		int *jnext;
		char *jend;
		XCmp_rc(ACC,CONST(val_null));
		XJump(JNeq,jnext);
		XMov_rc(ACC,CONST(val_false));
		XJump_near(jend);
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
			if( stack - nargs > 10 ) {
				pop_loop(stack - nargs);
			} else {
				pop(stack - nargs);
			}
			call(TAIL_CALL,nargs);
			// in case we return from a Primitive
			XRet();
		}
		break;
	case Ret:
		if( p > 10 ) {
			pop_loop(p);
		} else {
			pop(p);
		}
		XRet();
		break;
	case Add:
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
	case Mod:
		number_op(OP_MOD);
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
		XCall_m(alloc_object);
		stack_pop(Esp,1);
		break;
	case MakeArray:
		stack_pad(3);
		XPush_r(ACC);
		XPush_c(p + 1);
		XCall_m(alloc_array);
		XMov_rp(TMP,Esp,FIELD(1)); // tmp = saved acc
		XMov_pr(ACC,FIELD(1),TMP); // val_array_ptr(acc)[0] = tmp
		stack_pop_pad(2,3);
		if( p < 6 ) {
			i = 0;
			while( p > 0 ) {
				p--;
				i++;
				XMov_rp(TMP,SP,FIELD(p));
				XMov_pr(ACC,FIELD(i + 1),TMP);
				XMov_pc(SP,FIELD(p),0);
			}
			stack_pop(SP,i);
		} else {
			char *start;
			int *loop;
			XMov_rc(TMP2,(p+1));
			start = buf.c;
			XMov_rp(TMP,SP,FIELD(0));
			XMov_pc(SP,FIELD(0),0);
			XMov_xr(ACC,TMP2,4,TMP);
			stack_pop(SP,1);
			XSub_rc(TMP2,1);
			XCmp_rc(TMP2,1);
			XJump(JNeq,loop);
			*loop = (int)(start - buf.c);
		}
		break;
	case MakeArray2:
		stack_pad(3);
		XPush_r(ACC);
		XPush_c(p + 1);
		XCall_m(alloc_array);
		XMov_rp(TMP,Esp,FIELD(1)); // tmp = saved acc
		XMov_pr(ACC,FIELD(p+1),TMP); // val_array_ptr(acc)[p] = tmp
		stack_pop_pad(2,3);
		if( p < 6 ) {
			i = 0;
			while( p > 0 ) {
				p--;
				i++;
				XMov_rp(TMP,SP,FIELD(p));
				XMov_pr(ACC,FIELD(i),TMP);
				XMov_pc(SP,FIELD(p),0);
			}
			stack_pop(SP,i);
		} else {
			char *start;
			int *loop;
			XMov_rc(TMP2,p);
			start = buf.c;
			XMov_rp(TMP,SP,FIELD(0));
			XMov_pc(SP,FIELD(0),0);
			XMov_xr(ACC,TMP2,4,TMP);
			stack_pop(SP,1);
			XSub_rc(TMP2,1);
			XCmp_rc(TMP2,0);
			XJump(JNeq,loop);
			*loop = (int)(start - buf.c);
		}
		break;
	case MakeEnv:
		XPush_c(GET_PC());
		if( p >= MAX_ENV ) {
			XMov_rc(TMP,p);
			label(code->make_env_n);
		} else {
			label(code->make_env[p]);
		}
		stack_pop(Esp,1);
		break;
	case Last:
		XRet();
		break;
	case Apply: {
		int *jerr1, *jerr2, *jnext, *jcall1, *jcall2, *jdone;
		char *jend, *start;
		is_int(ACC,true,jerr1);
		XMov_rp(TMP,ACC,FIELD(0));
		XAnd_rc(TMP,TAG_MASK);
		XCmp_rb(TMP,VAL_FUNCTION);
		XJump(JNeq,jerr2);
		XMov_rp(TMP,ACC,FUNFIELD(nargs));

		// what do we do depending of the number of args ?
		XCmp_rb(TMP,p);
		XJump(JSignGt,jnext);
		XJump(JEq,jcall1);
		XCmp_rb(TMP,VAR_ARGS);
		XJump(JEq,jcall2);

		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(8,false); // $apply

		// build the apply
		PATCH_JUMP(jnext);
		XPush_r(ACC);
		XPush_r(TMP);
		XAdd_rc(TMP,1);
		XPush_r(TMP);
		XCall_m(alloc_array);
		stack_pop(Esp,1);
		XPop_r(TMP);
		XPop_r(TMP2);

		// fill the env array
		XMov_pr(ACC,FIELD(1),TMP2); // env[0] = f
		i = p;
		while( i-- > 0 ) {
			// env[*pc-i] = sp[i]
			XMov_rp(TMP2,SP,FIELD(i));
			XMov_pr(ACC,FIELD(1+p-i),TMP2);
		}
		pop(p);

		// fargs = fargs - *pc
		XSub_rc(TMP,p);
		// prepare stack for alloc_apply
		XPush_r(ACC);
		XPush_r(TMP);

		// fill the env array with null's
		XAdd_rc(ACC,(2+p) * 4 );
		start = buf.c;
		XCmp_rb(TMP,0);
		XJump(JEq,jdone);
		XMov_pc(ACC,FIELD(0),CONST(val_null));
		XAdd_rc(ACC,4);
		XSub_rc(TMP,1);
		// jump back to "start"
		{
			char *back;
			XJump_near(back);
			*back = (char)(start - (back + 1));
		}

		// call alloc_apply
		PATCH_JUMP(jdone);
		XCall_m(neko_alloc_apply);
		stack_pop(Esp,2);
		XJump_near(jend);

		// direct call
		PATCH_JUMP(jcall1);
		PATCH_JUMP(jcall2);
		call(NORMAL,p);

		PATCH_JUMP(jend);
		break;
		}
	case Trap: {
		// save some vm->start on the stack
		XMov_rp(TMP,VM,VMFIELD(start));
		XPush_r(TMP);
		XMov_rp(TMP,VM,VMFIELD(start)+FIELD(1));
		XPush_r(TMP);
		XMov_rp(TMP,VM,VMFIELD(start)+FIELD(2));
		XPush_r(TMP);
		XMov_rp(TMP,VM,VMFIELD(start)+FIELD(3));
		XPush_r(TMP);
		// save magic, ebp , esp and ret_pc
		XMov_pc(VM,VMFIELD(start),CONST(jit_handle_trap));
		XMov_pr(VM,VMFIELD(start)+FIELD(1),Ebp);
		XMov_pr(VM,VMFIELD(start)+FIELD(2),Esp);
		XMov_pc(VM,VMFIELD(start)+FIELD(3),-1);
		{
			jlist *t = (jlist*)alloc(sizeof(jlist));
			ctx->buf = buf;
			t->pos = POS() - 4;
			t->target = (int)((int_val*)(int_val)p - ctx->module->code);
			t->next = ctx->traps;
			ctx->traps = t;

		}
		// neko_setup_trap(vm)
		XPush_r(ACC);
		stack_pad(3);
		XPush_r(VM);
		begin_call();
		XCall_m(neko_setup_trap);
		end_call();
		stack_pop(Esp,1);
		stack_pad(-3);
		XPop_r(ACC);
		break;
		}
	case EndTrap: {
		// check spmax - trap = sp
		get_var_r(TMP,VSpMax);
		get_var_r(TMP2,VTrap);
		XShl_rc(TMP2,2);
		XSub_rr(TMP,TMP2);
		XCmp_rr(TMP,SP);
		XJump(JEq,jok);
		runtime_error(9,false); // Invalid End Trap
		PATCH_JUMP(jok);

		// restore VM jmp_buf
		XPop_r(TMP);
		XMov_pr(VM,VMFIELD(start)+FIELD(3),TMP);
		XPop_r(TMP);
		XMov_pr(VM,VMFIELD(start)+FIELD(2),TMP);
		XPop_r(TMP);
		XMov_pr(VM,VMFIELD(start)+FIELD(1),TMP);
		XPop_r(TMP);
		XMov_pr(VM,VMFIELD(start),TMP);

		// trap = val_int(sp[5])
		XMov_rp(TMP,SP,FIELD(5));
		XShr_rc(TMP,1);
		set_var_r(VTrap,TMP);
		pop(6);
		break;
		}
	case TypeOf: {
		int *jnot_int;
		char *jend;
		is_int(ACC,false,jnot_int);
		XMov_rc(ACC,CONST(alloc_int(1))); // t_int != VAL_INT
		XJump_near(jend);
		PATCH_JUMP(jnot_int);
		XMov_rp(TMP,ACC,FIELD(0));
		XAnd_rc(TMP,TAG_MASK);
		XMov_rc(TMP2,CONST(NEKO_TYPEOF));
		XMov_rx(ACC,TMP2,TMP,4);
		PATCH_JUMP(jend);
		break;
		}
	case Compare: {
		int *jint;
		char *jend;
		// val_compare(sp[0],acc)
		XPush_r(ACC);
		XMov_rp(TMP,SP,FIELD(0));
		XPush_r(TMP);
		begin_call();
		XCall_m(val_compare);
		end_call();
		stack_pop(Esp,2);

		XCmp_rc(ACC,invalid_comparison);
		XJump(JNeq,jint);
		XMov_rc(ACC,CONST(val_null));
		XJump_near(jend);
		PATCH_JUMP(jint);
		XShl_rc(ACC,1);
		XOr_rc(ACC,1);
		PATCH_JUMP(jend);
		pop(1);
		break;
		}
	case PhysCompare: {
		int *jeq, *jlow;
		char *jend1, *jend2;
		XMov_rp(TMP,SP,FIELD(0));
		pop(1);
		XCmp_rr(ACC,TMP);
		XJump(JEq,jeq);
		XJump(JSignLt,jlow);
		XMov_rc(ACC,CONST(alloc_int(-1)));
		XJump_near(jend1);
		PATCH_JUMP(jlow);
		XMov_rc(ACC,CONST(alloc_int(1)));
		XJump_near(jend2);
		PATCH_JUMP(jeq);
		XMov_rc(ACC,CONST(alloc_int(0)));
		PATCH_JUMP(jend1);
		PATCH_JUMP(jend2);
		break;
		}
	case Hash: {
		int *jerr1, *jerr2;
		char *jend;
		is_int(ACC,true,jerr1);
		XMov_rp(TMP,ACC,FIELD(0));
		XAnd_rc(TMP,TAG_MASK);
		XCmp_rb(TMP,VAL_STRING);
		XJump(JNeq,jerr2);
		begin_call();
		XAdd_rc(ACC,4); // val_string(acc)
		XPush_r(ACC);
		XCall_m(val_id);
		stack_pop(Esp,1);
		XShl_rc(ACC,1);
		XOr_rc(ACC,1);
		XJump_near(jend);
		PATCH_JUMP(jerr1);
		PATCH_JUMP(jerr2);
		runtime_error(10,false); // $hash
		PATCH_JUMP(jend);
		break;
		}
	case JumpTable: {
		int njumps = p / 2;
		int *jok1, *jok2;
		char *jnext1, *jnext2;
		is_int(ACC,true,jok1);
		XMov_rc(TMP2,njumps);
		XJump_near(jnext1);
		PATCH_JUMP(jok1);
		XCmp_rc(ACC,p);
		XJump(JLt,jok2);
		XMov_rc(TMP2,njumps);
		XJump_near(jnext2);
		PATCH_JUMP(jok2);
		XMov_rr(TMP2,ACC);
		XShr_rc(TMP2,1);
		PATCH_JUMP(jnext1);
		PATCH_JUMP(jnext2);
		get_var_r(TMP,VModule);
		// tmp = jit + tmp2 * 5
		XMov_rp(TMP,TMP,FIELD(0)); // m->jit
		XAdd_rr(TMP,TMP2);
		XShl_rc(TMP2,2);
		XAdd_rr(TMP,TMP2);

		ctx->debug_wait = njumps;
		ctx->buf = buf;
		{
			int add_size = 6;
			int small_add_size = 3;
			int jump_rsize = 2;
			int delta = POS() + add_size + jump_rsize;
			if( IS_SBYTE(delta) )
				delta += small_add_size - add_size;
			XAdd_rc(TMP,delta);
			XJump_r(TMP);
		}

		break;
		}
	case Loop:
		// nothing
		break;
	default:
		ERROR;
	}
	END_BUFFER;
}


#if defined(STACK_ALIGN_DEBUG) || defined(NEKO_JIT_DEBUG)
#	define MAX_OP_SIZE		1000
#	define MAX_BUF_SIZE		1000
#else
#	define MAX_OP_SIZE		298 // Apply(4) + label(stack_expand)
#	define MAX_BUF_SIZE		500
#endif

#define FILL_BUFFER(f,param,ptr) \
	{ \
		jit_ctx *ctx; \
		char *buf = alloc_private(MAX_BUF_SIZE); \
		int size; \
		ctx = jit_init_context(buf,MAX_BUF_SIZE); \
		f(ctx,param); \
		size = POS(); \
		buf = alloc_jit_mem(size); \
		code->ptr = buf; \
		memcpy(buf,ctx->baseptr,size); \
		ctx->buf.p = buf + size; \
		ctx->baseptr = buf; \
		jit_finalize_context(ctx); \
	}

#ifdef USE_MMAP

static void free_jit_mem( void *_p ) {
	int *p = (int*)_p - 1;
	munmap(p,*p);
}

static void free_jit_abstract( value v ) {
	free_jit_mem(val_data(v));
}

static char *alloc_jit_mem( int size ) {
	int *p;
	// add space for size
	size += sizeof(int);
	// round to next page
	size += (4096 - size%4096);
	p = (int*)mmap(NULL,size,PROT_READ|PROT_WRITE|PROT_EXEC,(MAP_PRIVATE|MAP_ANON),-1,0);
	if( p == (int*)-1 ) {
		buffer b = alloc_buffer("Failed to allocate JIT memory ");
		val_buffer(b,alloc_int(size>>10));
		val_buffer(b,alloc_string("KB"));
		val_throw(buffer_to_string(b));
	}
	*p = size;
	return (char*)(p + 1);
}

#else
#	define alloc_jit_mem	alloc_private
#endif

void neko_init_jit() {
	int nstrings = sizeof(cstrings) / sizeof(const char *);
	int i;
	strings = alloc_root(nstrings);
	for(i=0;i<nstrings;i++)
		strings[i] = alloc_string(cstrings[i]);
	code = (jit_code*)alloc_root(sizeof(jit_code) / sizeof(char*));
	FILL_BUFFER(jit_boot,NULL,boot);
	FILL_BUFFER(jit_trap,0,handle_trap);
	FILL_BUFFER(jit_stack_expand,0,stack_expand);
	FILL_BUFFER(jit_runtime_error,0,runtime_error);
	FILL_BUFFER(jit_invalid_access,0,invalid_access);
	FILL_BUFFER(jit_object_get,0,oo_get);
	FILL_BUFFER(jit_object_set,0,oo_set);
	for(i=0;i<NARGS;i++) {
		FILL_BUFFER(jit_call_jit_normal,i,call_normal_jit[i]);
		FILL_BUFFER(jit_call_jit_this,i,call_this_jit[i]);
		FILL_BUFFER(jit_call_jit_tail,i,call_tail_jit[i]);

		FILL_BUFFER(jit_call_prim_normal,i,call_normal_prim[i]);
		FILL_BUFFER(jit_call_prim_this,i,call_this_prim[i]);
		FILL_BUFFER(jit_call_prim_tail,i,call_tail_prim[i]);

		FILL_BUFFER(jit_call_fun_normal,i,call_normal_fun[i]);
		FILL_BUFFER(jit_call_fun_this,i,call_this_fun[i]);
		FILL_BUFFER(jit_call_fun_tail,i,call_tail_fun[i]);
	}
	FILL_BUFFER(jit_make_env,-1,make_env_n);
	for(i=0;i<MAX_ENV;i++) {
		FILL_BUFFER(jit_make_env,i,make_env[i]);
	}
	jit_boot_seq = code->boot;
	jit_handle_trap = code->handle_trap;
}

void neko_free_jit() {
#	ifdef USE_MMAP
	int i;
	for(i=0;i<sizeof(code)/sizeof(char*);i++)
        free_jit_mem(((char**)code)[i]);
#	endif
	free_root((value*)code);
	free_root(strings);
	code = NULL;
	strings = NULL;
	jit_boot_seq = NULL;
}

int neko_can_jit() {
	return 1;
}

static unsigned int next_function( neko_module *m, unsigned int k, int_val *faddr ) {
	while( k < m->nglobals && !val_is_function(m->globals[k]) )
		k++;
	if( k == m->nglobals ) {
		*faddr = -1;
		return 0;
	}
	*faddr = (int_val*)((vfunction*)m->globals[k])->addr - m->code;
	return k;
}

void neko_module_jit( neko_module *m ) {
	unsigned int i = 0;
	int_val faddr;
	unsigned int fcursor = next_function(m,0,&faddr);
	jit_ctx *ctx = jit_init_context(NULL,0);
	ctx->pos = (int*)tmp_alloc(sizeof(int)*(m->codesize + 1));
	ctx->module = m;
	while( i <= m->codesize ) {
		enum OPCODE op = m->code[i];
		int curpos = POS();
		ctx->pos[i] = curpos;
		ctx->curpc = i + 2;

		// resize buffer
		if( curpos + MAX_OP_SIZE > ctx->size ) {
			int nsize = ctx->size ? (ctx->size * 4) / 3 : ((m->codesize + 1) * 20);
			char *buf2;
			if( nsize - curpos < MAX_OP_SIZE ) nsize = curpos + MAX_OP_SIZE;
			buf2 = tmp_alloc(nsize);
			memcpy(buf2,ctx->baseptr,curpos);
			tmp_free(ctx->baseptr);
			ctx->baseptr = buf2;
			ctx->buf.p = buf2 + curpos;
			ctx->size = nsize;
		}

		// begin of function : check stack overflow
		if( faddr == i ) {
			INIT_BUFFER;
			label(code->stack_expand);
			END_BUFFER;
			fcursor = next_function(m,fcursor+1,&faddr);
		}

		// --------- debug ---------
#		ifdef NEKO_JIT_DEBUG
		if( ctx->debug_wait )
			ctx->debug_wait--;
		else {
			INIT_BUFFER;
			XPush_r(ACC);

			// val_print(acc)
			XPush_r(ACC);
			XCall_m_real(val_print);
			stack_pop(Esp,1);
			// val_print(pc_pos)
			XPush_c(CONST(alloc_int(i)));
			XCall_m_real(val_print_2);
			stack_pop(Esp,1);
			// val_print(alloc_int(spmax - sp))
			get_var_r(TMP,VSpMax);
			XSub_rr(TMP,SP);
			XShr_rc(TMP,1);
			XOr_rc(TMP,1);
			XPush_r(TMP);
			XCall_m_real(val_print_2);
			stack_pop(Esp,1);
			// val_print(alloc_int(csp - spmin))
			XMov_rp(TMP2,VM,VMFIELD(spmin));
			XMov_rr(TMP,CSP);
			XSub_rr(TMP,TMP2);
			XShr_rc(TMP,1);
			XOr_rc(TMP,1);
			XPush_r(TMP);
			XCall_m_real(val_print_3);
			stack_pop(Esp,1);

			XPop_r(ACC);
			END_BUFFER;
		}
#		endif

		i++;
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
		char *rbuf = alloc_jit_mem(csize);
		memcpy(rbuf,ctx->baseptr,csize);
		tmp_free(ctx->baseptr);
		ctx->baseptr = rbuf;
		ctx->buf.p = rbuf + csize;
		ctx->size = csize;
#		ifdef USE_MMAP
		m->jit_gc = alloc_abstract(NULL,rbuf);
		val_gc(m->jit_gc,free_jit_abstract);
#		endif
#		ifdef NEKO_JIT_DEBUG
		printf("Jit size = %d ( x%.1f )\n",csize,csize * 1.0 / ((m->codesize + 1) * 4));
#		endif
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
	tmp_free(ctx->pos);
}

#else // NEKO_JIT_ENABLE

char *jit_boot_seq = NULL;
char *jit_handle_trap = (char*)&jit_boot_seq;

void neko_init_jit() {
}

void neko_free_jit() {
}

int neko_can_jit() {
	return 0;
}

void neko_module_jit( neko_module *m ) {
}

#endif

/* ************************************************************************ */
