#include <neko.h>
#include <stdio.h>
#ifdef _WIN32
#	include <windows.h>
#endif
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
	value name;
	FILE *io;
	bool tmp;
} fio;

#define val_file(o)	((fio*)val_data(o))

DEFINE_KIND(k_file);

static value file_new( value cwd, value fname ) {
	value f;
	fio *io;
	buffer b;
	if( !val_is_string(cwd) || !val_is_string(fname) )
		return val_null;
	f = alloc_abstract(k_file, alloc(sizeof(fio)) );
	io = val_data(f);
	b = alloc_buffer(NULL);
	if( *val_string(f) == '~' ) {
#ifdef _WIN32
		char buf[MAX_PATH];
		GetTempPath(MAX_PATH,buf);
		buffer_append(b,buf);
#else
		buffer_append(b,"/tmp/");
#endif		
		val_buffer(b,fname);
		io->tmp = true;
	} else {
		val_buffer(b,cwd);
		val_buffer(b,fname);
	}
	io->name = buffer_to_string(b);
	io->io = NULL;	
	return f;
}

static value file_open( value o, value r ) {
	fio *f;
	val_check_kind(o,k_file);
	if( !val_is_string(r) )
		return val_false;
	f = val_file(o);
	if( f->io != NULL )
		fclose(f->io);
	f->io = fopen(val_string(f->name),val_string(r));
	return alloc_bool(f->io != NULL);
}

static value file_close( value o ) {	
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	if( f->io != NULL ) {
		fclose(f->io);
		f->io = NULL;
		return val_true;
	}
	return val_false;
}

static value file_contents( value o ) {
	value s;
	fio *f;
	FILE *io;
	int len;
	int p;
	val_check_kind(o,k_file);
	f = val_file(o);
	io = fopen(val_string(f->name),"rb");
	if( io == NULL )
		return val_null;
	fseek(io,0,SEEK_END);
	len = ftell(io);
	fseek(io,0,SEEK_SET);
	s = alloc_empty_string(len);
	p = 0;
	while( len > 0 ) {
		int d = fread((char*)val_string(s)+p,1,len,io);
		if( d <= 0 ) {
			fclose(io);
			return val_null;
		}
		p += d;
		len -= d;
	}	
	fclose(io);	
	return s;
}

static value file_write( value o, value s ) {
	fio *f;
	int p = 0;
	int len;
	val_check_kind(o,k_file);
	if( !val_is_string(s) )
		return val_null;
	f = val_file(o);
	if( f->io == NULL )
		return val_false;
	len = val_strlen(s);
	while( len > 0 ) {
		int d = fwrite(val_string(s)+p,1,len,f->io);
		if( d <= 0 )			
			return val_false;		
		p += d;
		len -= d;
	}
	return val_true;
}

static value file_read( value o, value n ) {
	fio *f;
	int p = 0;
	int len;
	value s;
	val_check_kind(o,k_file);
	if( !val_is_int(n) )
		return val_null;
	f = val_file(o);
	len = val_int(n);
	if( f->io == NULL )
		return val_false;
	s = alloc_empty_string(len);
	while( len > 0 ) {
		int d = fread((char*)val_string(s)+p,1,len,f->io);
		if( d <= 0 ) {
			val_set_length(s,p);
			break;
		}
		p += d;
		len -= d;
	}
	return s;
}

static value file_exists( value o ) {
	struct stat inf;
	val_check_kind(o,k_file);
	return alloc_bool( stat(val_string(val_file(o)->name),&inf) == 0 );
}

static value file_delete( value o ) {
	val_check_kind(o,k_file);
	return alloc_bool( unlink(val_string(val_file(o)->name)) == 0 );
}

static value file_path( value o ) {
	val_check_kind(o,k_file);
	return val_file(o)->name;
}

DEFINE_PRIM(file_new,2);
DEFINE_PRIM(file_open,2);
DEFINE_PRIM(file_close,1);
DEFINE_PRIM(file_contents,1);
DEFINE_PRIM(file_write,2);
DEFINE_PRIM(file_read,2);
DEFINE_PRIM(file_exists,1);
DEFINE_PRIM(file_delete,1);
DEFINE_PRIM(file_path,1);

