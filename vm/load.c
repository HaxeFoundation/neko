/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include <stdlib.h>
#include "load.h"
#define PARAMETER_TABLE
#include "opcodes.h"

#define MAXSIZE 0x100
#define ERROR() { return NULL; }

extern value *builtins;

static int read_string( reader r, readp p, char *buf ) {
	int i = 0;
	char c;
	while( i < MAXSIZE ) {
		if( r(p,&c,1) != 1 )
			return -1;
		buf[i++] = c;
		if( c == 0 )
			return i;
	}
	return -1;
}

neko_module *neko_module_load( reader r, readp p ) {
	unsigned int i;
	unsigned int itmp;
	unsigned char t;
	unsigned short stmp;
	char *tmp = NULL;
	neko_module *m = (neko_module*)alloc(sizeof(neko_module));
	if( r(p,&itmp,4) != 4 || itmp != 0x4F4B454E )
		ERROR();
	if( r(p,&m->nglobals,4) != 4 )
		ERROR();
	if( r(p,&m->nfields,4) != 4 )
		ERROR();
	if( r(p,&m->codesize,4) != 4 )
		ERROR();
	if( m->nglobals < 0 || m->nglobals > 0xFFFF || m->nfields < 0 || m->nfields > 0xFFFF || m->codesize < 0 || m->codesize > 0xFFFFF )
		ERROR();
	tmp = alloc_abstract(sizeof(char)*((m->codesize>=MAXSIZE)?(m->codesize+1):MAXSIZE));
	m->globals = (value*)alloc(m->nglobals * sizeof(value));
	m->fields = (value*)alloc(sizeof(value*)*m->nfields);
	m->code = (int*)alloc_abstract(sizeof(int)*(m->codesize+1));
	// Init global table
	for(i=0;i<m->nglobals;i++) {
		if( r(p,&t,1) != 1 )
			ERROR();
		switch( t ) {
		case 1:
			if( read_string(r,p,tmp) == -1 )
				ERROR();
			m->globals[i] = alloc_string(tmp);
			break;
		case 2:
			if( r(p,&itmp,4) != 4 )
				ERROR();
			if( (itmp & 0xFFFFFF) >= m->codesize )
				ERROR();
			m->globals[i] = alloc_function((void*)(itmp&0xFFFFFF),(itmp >> 24));
			m->globals[i]->t = VAL_FUNCTION;
			break;
		case 3:
			if( r(p,&stmp,2) != 2 )
				ERROR();
			if( r(p,tmp,stmp) != stmp )
				ERROR();
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
		if( r(p,&t,1) != 1 )
			ERROR();
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
			if( r(p,&t,1) != 1 )
				ERROR();
			tmp[i] = 0;
			m->code[i++] = t;
			break;
		case 3:
			m->code[i++] = (t >> 2);
			if( r(p,&itmp,4) != 4 )
				ERROR();
			tmp[i] = 0;
			m->code[i++] = itmp;
			break;
		}
	}
	tmp[i] = 1;
	m->code[i] = Ret;
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
			m->code[i+1] = (int)val_int((int)itmp);
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

/* ************************************************************************ */
