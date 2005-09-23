/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
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
#include <neko.h>
#include <stdio.h>
#ifdef _WIN32
#	include <windows.h>
#endif

typedef struct {
	value name;
	FILE *io;
} fio;

#define val_file(o)	((fio*)val_data(o))

DEFINE_KIND(k_file);

static void file_error( const char *msg, fio *f ) {
	value a = alloc_array(2);
	val_array_ptr(a)[0] = alloc_string(msg);
	val_array_ptr(a)[1] = alloc_string(val_string(f->name));
	val_throw(a);
}

static value file_open( value name, value r ) {
	fio *f;
	val_check(name,string);
	val_check(r,string);
	f = (fio*)alloc_private(sizeof(fio));
	f->name = alloc_string(val_string(name));
	f->io = fopen(val_string(name),val_string(r));
	if( f->io == NULL )
		file_error("file_open",f);
	return alloc_abstract(k_file,f);
}

static value file_close( value o ) {	
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	fclose(f->io);
	val_kind(o) = NULL;
	return val_true;
}

static value file_path( value o ) {
	val_check_kind(o,k_file);
	return alloc_string(val_string(val_file(o)->name));
}

static value file_write( value o, value s, value pp, value n ) {
	int p, len;
	fio *f;
	val_check_kind(o,k_file);
	val_check(s,string);
	val_check(pp,int);
	val_check(n,int);
	f = val_file(o);
	p = val_int(pp);
	len = val_int(n);
	if( p < 0 || len < 0 || p > val_strlen(s) || p + len > val_strlen(s) )
		type_error();
	while( len > 0 ) {
		int d = fwrite(val_string(s)+p,1,len,f->io);
		if( d <= 0 )
			file_error("file_write",f);
		p += d;
		len -= d;
	}
	return n;
}

static value file_read( value o, value s, value pp, value n ) {
	fio *f;
	int p;
	int len;
	val_check_kind(o,k_file);
	val_check(s,string);
	val_check(pp,int);
	val_check(n,int);
	f = val_file(o);
	p = val_int(pp);
	len = val_int(n);
	if( p < 0 || len < 0 || p > val_strlen(s) || p + len > val_strlen(s) )
		type_error();
	while( len > 0 ) {
		int d = fread((char*)val_string(s)+p,1,len,f->io);
		if( d <= 0 ) {
			if( p == 0 )
				file_error("file_read",f);
			return alloc_int(val_int(n) - len);
		}
		p += d;
		len -= d;
	}
	return n;
}

static value file_write_char( value o, value c ) {
	unsigned char cc;
	fio *f;
	val_check(c,int);
	val_check_kind(o,k_file);
	if( val_int(c) < 0 || val_int(c) > 255 )
		type_error();
	cc = (char)val_int(c);
	f = val_file(o);
	if( fwrite(&cc,1,1,f->io) != 1 )
		file_error("file_write_char",f);
	return val_true;
}

static value file_read_char( value o ) {
	unsigned char cc;
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);	
	if( fread(&cc,1,1,f->io) != 1 )
		file_error("file_read_char",f);
	return alloc_int(cc);
}

static value file_seek( value o, value pos, value kind ) {
	fio *f;
	val_check_kind(o,k_file);
	val_check(pos,int);
	val_check(kind,int);
	f = val_file(o);
	if( fseek(f->io,val_int(pos),val_int(kind)) != 0 )
		file_error("file_seek",f);
	return val_true;
}

static value file_tell( value o ) {
	int p;
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	p = ftell(f->io);
	if( p == -1 )
		file_error("file_tell",f);
	return alloc_int(p);
}

static value file_eof( value o ) {
	val_check_kind(o,k_file);
	return alloc_bool( feof(val_file(o)->io) );
}

static value file_flush( value o ) {
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	if( fflush( f->io ) != 0 )
		file_error("file_flush",f);
	return val_true;
}

static value file_contents( value name ) {
	value s;
	fio f;
	int len;
	int p;
	val_check(name,string);
	f.name = name;
	f.io = fopen(val_string(name),"rb");
	if( f.io == NULL )
		file_error("file_contents",&f);
	fseek(f.io,0,SEEK_END);
	len = ftell(f.io);
	fseek(f.io,0,SEEK_SET);
	s = alloc_empty_string(len);
	p = 0;
	while( len > 0 ) {
		int d = fread((char*)val_string(s)+p,1,len,f.io);
		if( d <= 0 )
			file_error("file_contents",&f);
		p += d;
		len -= d;
	}	
	fclose(f.io);
	return s;
}

#define MAKE_STDIO(k) \
	static value file_##k() { \
		fio *f; \
		f = (fio*)alloc_private(sizeof(fio)); \
		f->name = alloc_string(#k); \
		f->io = k; \
		return alloc_abstract(k_file,f); \
	} \
	DEFINE_PRIM(file_##k,0);

MAKE_STDIO(stdin);
MAKE_STDIO(stdout);
MAKE_STDIO(stderr);

DEFINE_PRIM(file_open,2);
DEFINE_PRIM(file_close,1);
DEFINE_PRIM(file_path,1);
DEFINE_PRIM(file_write,4);
DEFINE_PRIM(file_read,4);
DEFINE_PRIM(file_write_char,2);
DEFINE_PRIM(file_read_char,1);
DEFINE_PRIM(file_seek,3);
DEFINE_PRIM(file_tell,1);
DEFINE_PRIM(file_eof,1);
DEFINE_PRIM(file_flush,1);
DEFINE_PRIM(file_contents,1);

/* ************************************************************************ */
