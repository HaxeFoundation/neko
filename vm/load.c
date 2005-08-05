/* ************************************************************************ */
/*																			*/
/*	Neko VM source															*/
/*  (c)2005 Nicolas Cannasse												*/
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#ifdef __linux__
#	include <dlfcn.h>
#endif
#include "vmcontext.h"
#include "load.h"
#include "interp.h"
#define PARAMETER_TABLE
#include "opcodes.h"

DEFINE_KIND(k_loader);
DEFINE_KIND(k_module);

#define MAXSIZE 0x100
#define ERROR() { return NULL; }
#define READ(buf,len) if( r(p,buf,len) == -1 ) ERROR()

extern field id_loader;
extern field id_exports;
extern field id_data;
extern field id_module;
extern value *neko_builtins;
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

static int builtin_id( neko_module *m, field id ) {
	value f = val_field(neko_builtins[NBUILTINS],id);
	if( val_is_null(f) ) {
		unsigned int i;
		for(i=0;i<m->nfields;i++)
			if( val_id(val_string(m->fields[i])) == id ) {
				buffer b = alloc_buffer("Builtin not found : ");
				val_buffer(b,m->fields[i]);
				val_throw(buffer_to_string(b));
			}
		return 0;
	}
	return val_int(f);
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
	tmp = alloc_private(sizeof(char)*(((m->codesize+1)>MAXSIZE)?(m->codesize+1):MAXSIZE));
	m->globals = (value*)alloc(m->nglobals * sizeof(value));
	m->fields = (value*)alloc(sizeof(value*)*m->nfields);
	m->code = (int*)alloc_private(sizeof(int)*(m->codesize+1));
	m->loader = loader;
	m->exports = alloc_object(NULL);
	alloc_field(m->exports,id_module,alloc_abstract(k_module,m));
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
			if( stmp > MAXSIZE ) {
				char *ttmp;
				ttmp = alloc_private(stmp);
				READ(ttmp,stmp);
				m->globals[i] = copy_string(ttmp,stmp);
			} else {
				READ(tmp,stmp);
				m->globals[i] = copy_string(tmp,stmp);
			}
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
			if( (field)itmp == id_loader )
				m->code[i+1] = (int)loader;
			else if( (field)itmp == id_exports )
				m->code[i+1] = (int)m->exports;
			else {
				itmp = builtin_id(m,(field)itmp);
				if(	itmp >= NBUILTINS )
					ERROR();
				m->code[i+1] = (int)neko_builtins[itmp];
			}
			break;
		case Call:
		case ObjCall:
			if( itmp > CALL_MAX_ARGS )
				val_throw(alloc_string("Too many arguments for a call"));
			break;
		case MakeEnv:
			if( itmp > 0xFF )
				val_throw(alloc_string("Too much big environment"));
			break;
		case MakeArray:
			if( itmp > 0x10000 )
				val_throw(alloc_string("Too much big array"));
			break;
		}
		if( !tmp[i+1] )
			i++;
	}
	return m;
}

static value default_loadprim( value prim, value nargs ) {
	value o = val_this();
	value data;
	if( !val_is_object(o) )
		return val_null;
	data = val_field(o,id_data);
	val_check_kind(data,k_loader);
	if( !val_is_string(prim) || !val_is_int(nargs) || val_int(nargs) > 10 || val_int(nargs) < -1 )
		return val_null;
	{
		loader *l = (loader*)val_data(data);
		void *ptr = l->p(val_string(prim),val_int(nargs),&l->custom);
		if( ptr == NULL ) {
			buffer b = alloc_buffer("Primitive not found : ");
			val_buffer(b,prim);
			val_throw(buffer_to_string(b));
		}
		return alloc_function(ptr,val_int(nargs));
	}
}

static value default_loadmodule( value mname, value this ) {
	value o = val_this();
	value data;
	if( !val_is_object(o) )
		return val_null;
	data = val_field(o,id_data);
	val_check_kind(data,k_loader);
	if( !val_is_string(mname) || !val_is_object(this) )
		return val_null;
	{
		loader *l = (loader*)val_data(data);
		reader r;
		readp p;
		neko_module *m;
		neko_vm *vm;
		field mid = val_id(val_string(mname));
		value v = val_field(l->cache,mid);
		if( !val_is_null(v) )
			return v;
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
		m->name = alloc_string(val_string(mname));
		vm = neko_vm_current();
		alloc_field(l->cache,mid,m->exports);
		neko_vm_execute(vm,m);
		return alloc_object(m->exports);
	}
}

#ifdef _WIN32
#	undef ERROR
#	include <windows.h>
#	define dlopen(l,p)		(void*)LoadLibrary(l)
#	define dlsym(h,n)		GetProcAddress((HANDLE)h,n)
#endif

static int file_reader( readp p, void *buf, int size ) {
	int len = 0;
	while( size > 0 ) {
		int l = fread(buf,1,size,(FILE*)p);
		if( l <= 0 )
			return len;
		size -= l;
		len += l;
		buf = (char*)buf+l;
	}
	return len;
}

EXTERN int neko_default_load_module( const char *mname, reader *r, readp *p ) {
	FILE *f;
	value fname = neko_select_file(mname,neko_vm_current()->env,".n");
	f = fopen(val_string(fname),"rb");
	if( f == NULL )
		return 0;
	*r = file_reader;
	*p = f;
	return 1;
}

EXTERN void neko_default_load_done( readp p ) {
	fclose(p);
}

typedef struct _liblist {
	char *name;
	void *handle;
	struct _liblist *next;
} liblist;

typedef value (*PRIM0)();

EXTERN void *neko_default_load_primitive( const char *prim, int nargs, void **custom ) {
	char *pos = strchr(prim,'@');
	int len;	
	liblist *l;
	PRIM0 ptr;
	if( pos == NULL )
		return NULL;
	l = (liblist*)*custom;
	*pos = 0;
	len = strlen(prim) + 1;
	while( l != NULL ) {
		if( memcmp(l->name,prim,len) == 0 )
			break;
		l = l->next;
	}
	if( l == NULL ) {
		void *h;
		value pname;
		pname = neko_select_file(prim,neko_vm_current()->env,".ndll");
		h = dlopen(val_string(pname),RTLD_LAZY);
		if( h == NULL ) {
			*pos = '@';
			return NULL;
		}
		l = (liblist*)alloc(sizeof(liblist));
		l->handle = h;
		l->name = alloc(len);
		memcpy(l->name,prim,len);
		l->next = (liblist*)*custom;
		*custom = l;
		ptr = (PRIM0)dlsym(l->handle,"__neko_entry_point");
		if( ptr != NULL )
			((PRIM0)ptr())();
	}
	*pos++ = '@';
	{
		char buf[100];
		if( strlen(pos) > 90 )
			return NULL;
		if( nargs == VAR_ARGS )
			sprintf(buf,"%s__MULT",pos);
		else
			sprintf(buf,"%s__%d",pos,nargs);
		ptr = (PRIM0)dlsym(l->handle,buf);
		if( ptr == NULL )
			return NULL;
		return ptr();
	}
}

EXTERN value neko_init_path( const char *path ) {
	value l = val_null, tmp;
	char *p;
	if( !path )
		return val_null;
	while( true ) {
		p = strchr(path,';');
		if( p != NULL )
			*p = 0;
		tmp = alloc_array(2);
		if( (p && p[-1] != '/' && p[-1] != '\\') || (!p && path[strlen(path)-1] != '/' && path[strlen(path)-1] != '\\') ) {
			buffer b = alloc_buffer(path);
			char c = '/';
			buffer_append_sub(b,&c,1);
			val_array_ptr(tmp)[0] = buffer_to_string(b);
		} else
			val_array_ptr(tmp)[0] = alloc_string(path);
		val_array_ptr(tmp)[1] = l;
		l = tmp;
		if( p != NULL )
			*p = ';';
		else
			break;
		path = p+1;
	}
	return l;
}

EXTERN value neko_select_file( const char *file, value path, const char *ext ) {
	struct stat s;
	value ff;
	buffer b = alloc_buffer(file);
	buffer_append(b,ext);
	ff = buffer_to_string(b);
	if( stat(val_string(ff),&s) == 0 )
		return ff;
	while( val_is_array(path) && val_array_size(path) == 2 ) {
		value p = val_array_ptr(path)[0];
		buffer b = alloc_buffer(NULL);
		path = val_array_ptr(path)[1];
		val_buffer(b,p);
		val_buffer(b,ff);
		p = buffer_to_string(b);
		if( stat(val_string(p),&s) == 0 )
			return p;
	}
	return ff;
}

EXTERN value neko_default_loader( loader *l ) {
	value o = alloc_object(NULL);
	value tmp;
	if( l == NULL ) {
		l = (loader*)alloc(sizeof(loader));
		l->l = neko_default_load_module;
		l->d = neko_default_load_done;
		l->p = neko_default_load_primitive;
		l->custom = NULL;
		l->paths = neko_init_path(getenv("NEKOPATH"));
		l->cache = alloc_object(NULL);
	}
	alloc_field(o,id_data,alloc_abstract(k_loader,l));
	tmp = alloc_function(default_loadprim,2);
	((vfunction*)tmp)->env = l->paths;
	alloc_field(o,val_id("loadprim"),tmp);
	tmp = alloc_function(default_loadmodule,2);
	((vfunction*)tmp)->env = l->paths;
	alloc_field(o,val_id("loadmodule"),tmp);
	return o;
}

/* ************************************************************************ */
