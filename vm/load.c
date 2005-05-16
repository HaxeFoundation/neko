/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "load.h"
#include "opcodes.h"

#define MAXSIZE 0x100
#define ERROR() { free(tmp); neko_module_free(m); return NULL; }

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

void neko_module_free( neko_module *m ) {
	unsigned int i;
	if( m->fields != NULL ) {
		for(i=0;i<m->nfields;i++)
			free(m->fields[i]);
		free(m->fields);
	}
	free(m->code);
	free_root(m->globals);
}

neko_module *neko_load( reader r, readp p ) {
	unsigned int i;
	unsigned int itmp;
	unsigned char t;
	unsigned short stmp;
	char *tmp = NULL;
	neko_module *m = malloc(sizeof(neko_module));
	memset(m,0,sizeof(neko_module));
	if( r(p,&m->nglobals,4) != 4 )
		ERROR();
	if( r(p,&m->nfields,4) != 4 )
		ERROR();
	if( r(p,&m->codesize,4) != 4 )
		ERROR();
	if( m->nglobals < 0 || m->nglobals > 0xFFFF || m->nfields < 0 || m->nfields > 0xFFFF || m->codesize < 0 || m->codesize > 0xFFFFF )
		ERROR();
	tmp = malloc(sizeof(char)*((m->codesize>=MAXSIZE)?(m->codesize+1):MAXSIZE));
	m->globals = alloc_root(m->nglobals);
	m->fields = malloc(sizeof(char*)*m->nfields);
	m->code = malloc(sizeof(unsigned int)*(m->codesize+1));
	memset(m->fields,0,sizeof(char*)*m->nfields);
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
		m->fields[i] = strdup(tmp);
	}
	i = 0;
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
	for(i=0;i<m->nglobals;i++) {
		vfunction *f = (vfunction*)m->globals[i];
		if( val_type(f) == VAL_FUNCTION ) {
			if( (unsigned int)f->addr >= m->codesize || !tmp[(unsigned int)f->addr]  )
				ERROR();
		}
	}
	for(i=0;i<m->codesize;i++) {
		switch( m->code[i] ) {
		case AccGlobal:
		case SetGlobal:
			if( m->code[i+1] >= m->nglobals )
				ERROR();
			break;
		case Jump:
		case JumpIf:
		case JumpIfNot:
		case Trap:
			itmp = (i+2+(int)m->code[i+1]);
			if( itmp > m->codesize || !tmp[itmp] )
				ERROR();
			break;
		}
		if( !tmp[i+1] )
			i++;
	}
	return m;
}

/* ************************************************************************ */
