/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include <stdlib.h>
#include "load.h"
#include "interp.h"
#define PARAMETER_TABLE
#include "opcodes.h"

#define t_loader				((otype)0x00000003)

#define MAXSIZE 0x100
#define ERROR() { return NULL; }
#define READ(buf,len) if( r(p,buf,len) == -1 ) ERROR()

extern value *builtins;
extern value alloc_module_function( void *m, int pos, int nargs );

static int read_string( reader r, readp p, char *buf ) {
	int i = 0;
	char c;
	while( i < MAXSIZE ) {
		if( r(p,&c,1) == -1 )
			return -1;
		buf[i++] = c;
		if( c == 0 )
			return i;
	}
	return -1;
}

static neko_module *neko_module_read( reader r, readp p, value loader ) {
	unsigned int i;
	unsigned int itmp;
	unsigned char t;
	unsigned short stmp;
	char *tmp = NULL;
	neko_module *m = (neko_module*)alloc(sizeof(neko_module));
	READ(&itmp,4);
	if( itmp != 0x4F4B454E )
		ERROR();
	READ(&m->nglobals,4);
	READ(&m->nfields,4);
	READ(&m->codesize,4);
	if( m->nglobals < 0 || m->nglobals > 0xFFFF || m->nfields < 0 || m->nfields > 0xFFFF || m->codesize < 0 || m->codesize > 0xFFFFF )
		ERROR();
	tmp = alloc_abstract(sizeof(char)*(((m->codesize+1)>MAXSIZE)?(m->codesize+1):MAXSIZE));
	m->globals = (value*)alloc(m->nglobals * sizeof(value));
	m->fields = (value*)alloc(sizeof(value*)*m->nfields);
	m->code = (int*)alloc_abstract(sizeof(int)*(m->codesize+1));
	m->loader = loader;
	m->exports = alloc_object(NULL);
	// Init global table
	for(i=0;i<m->nglobals;i++) {
		READ(&t,1);
		switch( t ) {
		case 1:
			if( read_string(r,p,tmp) == -1 )
				ERROR();
			m->globals[i] = alloc_string(tmp);
			break;
		case 2:
			READ(&itmp,4);
			if( (itmp & 0xFFFFFF) >= m->codesize )
				ERROR();
			m->globals[i] = alloc_module_function(m,(itmp&0xFFFFFF),(itmp >> 24));
			break;
		case 3:
			READ(&stmp,2);
			if( stmp > MAXSIZE )
				ERROR();
			READ(tmp,stmp);
			m->globals[i] = copy_string(tmp,stmp);
			break;
		case 4:
			if( read_string(r,p,tmp) == -1 )
				ERROR();
			m->globals[i] = alloc_float( atof(tmp) );
			break;
		default:
			ERROR();
			break;
		}
	}
	for(i=0;i<m->nfields;i++) {
		if( read_string(r,p,tmp) == -1 )
			ERROR();
		m->fields[i] = alloc_string(tmp);
	}
	i = 0;
	// Unpack opcodes
	while( i < m->codesize ) {
		READ(&t,1);
		tmp[i] = 1;
		switch( t & 3 ) {
		case 0:
			m->code[i++] = (t >> 2);
			break;
		case 1:
			m->code[i++] = (t >> 3);
			tmp[i] = 0;
			m->code[i++] = (t >> 2) & 1;
			break;
		case 2:
			m->code[i++] = (t >> 2);
			READ(&t,1);
			tmp[i] = 0;
			m->code[i++] = t;
			break;
		case 3:
			m->code[i++] = (t >> 2);
			READ(&itmp,4);
			tmp[i] = 0;
			m->code[i++] = itmp;
			break;
		}
	}
	tmp[i] = 1;
	m->code[i] = Last;
	// Check globals
	for(i=0;i<m->nglobals;i++) {
		vfunction *f = (vfunction*)m->globals[i];
		if( val_type(f) == VAL_FUNCTION ) {
			if( (unsigned int)f->addr >= m->codesize || !tmp[(unsigned int)f->addr]  )
				ERROR();
			f->addr = m->code + (int)f->addr;
		}
	}
	// Check bytecode
	for(i=0;i<m->codesize;i++) {
		int c = m->code[i];
		itmp = m->code[i+1];
		if( c >= Last || tmp[i+1] == parameter_table[c] )
			ERROR();
		// Additional checks and optimizations
		switch( m->code[i] ) {
		case AccGlobal:
		case SetGlobal:
			if( itmp >= m->nglobals )
				ERROR();
			m->code[i+1] = (int)(m->globals + itmp);
			break;
		case Jump:
		case JumpIf:
		case JumpIfNot:
		case Trap:
			itmp += i;
			if( itmp > m->codesize || !tmp[itmp] )
				ERROR();
			m->code[i+1] = (int)(m->code + itmp);
			break;
		case AccInt:
			m->code[i+1] = (int)alloc_int((int)itmp);
			break;
		case AccStack:
		case SetStack:
			if( ((int)itmp) < 0 )
				ERROR();
			if( itmp < STACK_DELTA )
				m->code[i] = (m->code[i] == AccStack)?AccStackFast:SetStackFast;
			break;
		case Ret:
		case Pop:
		case AccEnv:
		case SetEnv:
			if( ((int)itmp) < 0 )
				ERROR();
			break;
		case AccBuiltin:
			if( itmp >= NBUILTINS )
				ERROR();
			if( itmp == LOADER_BUILTIN )
				m->code[i+1] = (int)loader;
			else if( itmp == EXPORTS_BUILTIN )
				m->code[i+1] = (int)m->exports;
			else
				m->code[i+1] = (int)builtins[itmp];
			break;
		case Call:
		case ObjCall:
			if( itmp > CALL_MAX_ARGS )
				ERROR();
			break;
		case MakeEnv:
			if( itmp > 0xFF )
				ERROR();
			break;
		}
		if( !tmp[i+1] )
			i++;
	}
	return m;
}

static value neko_default_loadprim( value prim ) {
	value o = val_this();
	val_check_obj(o,t_loader);
	if( !val_is_string(prim) )
		return val_null;
	val_throw(prim);
	return val_null;
}

static value neko_default_loadmodule( value mname, value this ) {
	value o = val_this();
	val_check_obj(o,t_loader);
	if( !val_is_string(mname) || !val_is_object(this) )
		return val_null;
	{
		loader *l = (loader*)val_odata(o);
		reader r;
		readp p;
		neko_module *m;
		neko_vm *vm;
		if( !l->l(val_string(mname),&r,&p) ) {
			buffer b = alloc_buffer("Module not found : ");
			val_buffer(b,mname);
			val_throw(buffer_to_string(b));
		}
		m = neko_module_read(r,p,this);
		l->d(p);
		if( m == NULL )  {
			buffer b = alloc_buffer("Invalid module : ");
			val_buffer(b,mname);
			val_throw(buffer_to_string(b));
		}
		vm = neko_vm_current();
		neko_vm_execute(vm,m);
		return m->exports;
	}	
}

value neko_default_loader( loader *l ) {
	value o = alloc_object(NULL);
	val_otype(o) = t_loader;
	val_odata(o) = (value)l;
	alloc_field(o,val_id("loadprim"),alloc_function(neko_default_loadprim,1));
	alloc_field(o,val_id("loadmodule"),alloc_function(neko_default_loadmodule,2));
	return o;
}

/* ************************************************************************ */
