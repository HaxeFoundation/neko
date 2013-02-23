/*
 * Copyright (C)2005-2012 Haxe Foundation
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
#include <neko.h>
#include <stdio.h>
#ifdef NEKO_WINDOWS
#	include <windows.h>
#endif

/**
	<doc>
	<h1>File</h1>
	<p>
	The file api can be used for different kind of file I/O.
	</p>
	</doc>
**/

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

/**
	file_open : f:string -> r:string -> 'file
	<doc>
	Call the C function [fopen] with the file path and access rights. 
	Return the opened file or throw an exception if the file couldn't be open.
	</doc>
**/
static value file_open( value name, value r ) {
	fio *f;
	val_check(name,string);
	val_check(r,string);
	f = (fio*)alloc(sizeof(fio));
	f->name = alloc_string(val_string(name));
	f->io = fopen(val_string(name),val_string(r));
	if( f->io == NULL )
		file_error("file_open",f);
	return alloc_abstract(k_file,f);
}

/**
	file_close : 'file -> void
	<doc>Close an file. Any other operations on this file will fail</doc> 
**/
static value file_close( value o ) {	
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	fclose(f->io);
	val_kind(o) = NULL;
	return val_null;
}

/**
	file_name : 'file -> string
	<doc>Return the name of the file which was opened</doc>
**/
static value file_name( value o ) {
	val_check_kind(o,k_file);
	return alloc_string(val_string(val_file(o)->name));
}

/**
	file_write : 'file -> s:string -> p:int -> l:int -> int
	<doc>
	Write up to [l] chars of string [s] starting at position [p]. 
	Returns the number of chars written which is >= 0.
	</doc>
**/
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
		neko_error();
	while( len > 0 ) {
		int d;
		POSIX_LABEL(file_write_again);
		d = (int)fwrite(val_string(s)+p,1,len,f->io);
		if( d <= 0 ) {
			HANDLE_FINTR(f->io,file_write_again);
			file_error("file_write",f);
		}
		p += d;
		len -= d;
	}
	return n;
}

/**
	file_read : 'file -> s:string -> p:int -> l:int -> int
	<doc>
	Read up to [l] chars into the string [s] starting at position [p].
	Returns the number of chars readed which is > 0 (or 0 if l == 0).
	</doc>
**/
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
		neko_error();
	while( len > 0 ) {
		int d;
		POSIX_LABEL(file_read_again);
		d = (int)fread((char*)val_string(s)+p,1,len,f->io);
		if( d <= 0 ) {
			int size = val_int(n) - len;
			HANDLE_FINTR(f->io,file_read_again);
			if( size == 0 )
				file_error("file_read",f);
			return alloc_int(size);
		}
		p += d;
		len -= d;
	}
	return n;
}

/**
	file_write_char : 'file -> c:int -> void
	<doc>Write the char [c]. Error if [c] outside of the range 0..255</doc>
**/
static value file_write_char( value o, value c ) {
	unsigned char cc;
	fio *f;
	val_check(c,int);
	val_check_kind(o,k_file);
	if( val_int(c) < 0 || val_int(c) > 255 )
		neko_error();
	cc = (char)val_int(c);
	f = val_file(o);
	POSIX_LABEL(write_char_again);
	if( fwrite(&cc,1,1,f->io) != 1 ) {
		HANDLE_FINTR(f->io,write_char_again);
		file_error("file_write_char",f);
	}
	return val_null;
}

/**
	file_read_char : 'file -> int
	<doc>Read a char from the file. Exception on error</doc>
**/
static value file_read_char( value o ) {
	unsigned char cc;
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	POSIX_LABEL(read_char_again);
	if( fread(&cc,1,1,f->io) != 1 ) {
		HANDLE_FINTR(f->io,read_char_again);
		file_error("file_read_char",f);
	}
	return alloc_int(cc);
}

/**
	file_seek : 'file -> pos:int -> mode:int -> void
	<doc>Use [fseek] to move the file pointer.</doc>
**/
static value file_seek( value o, value pos, value kind ) {
	fio *f;
	val_check_kind(o,k_file);
	val_check(pos,int);
	val_check(kind,int);
	f = val_file(o);
	if( fseek(f->io,val_int(pos),val_int(kind)) != 0 )
		file_error("file_seek",f);
	return val_null;
}

/**
	file_tell : 'file -> int
	<doc>Return the current position in the file</doc>
**/
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

/**
	file_eof : 'file -> bool
	<doc>Tell if we have reached the end of the file</doc>
**/
static value file_eof( value o ) {
	val_check_kind(o,k_file);
	return alloc_bool( feof(val_file(o)->io) );
}

/**
	file_flush : 'file -> void
	<doc>Flush the file buffer</doc>
**/
static value file_flush( value o ) {
	fio *f;
	val_check_kind(o,k_file);
	f = val_file(o);
	if( fflush( f->io ) != 0 )
		file_error("file_flush",f);
	return val_true;
}

/**
	file_contents : f:string -> string
	<doc>Read the content of the file [f] and return it.</doc>
**/
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
		int d;
		POSIX_LABEL(file_contents);
		d = (int)fread((char*)val_string(s)+p,1,len,f.io);
		if( d <= 0 ) {
			HANDLE_FINTR(f.io,file_contents);
			fclose(f.io);
			file_error("file_contents",&f);
		}
		p += d;
		len -= d;
	}	
	fclose(f.io);
	return s;
}

#define MAKE_STDIO(k) \
	static value file_##k() { \
		fio *f; \
		f = (fio*)alloc(sizeof(fio)); \
		f->name = alloc_string(#k); \
		f->io = k; \
		return alloc_abstract(k_file,f); \
	} \
	DEFINE_PRIM(file_##k,0);

/**
	file_stdin : void -> 'file
	<doc>The standard input</doc>
**/
MAKE_STDIO(stdin);
/**
	file_stdout : void -> 'file
	<doc>The standard output</doc>
**/
MAKE_STDIO(stdout);
/**
	file_stderr : void -> 'file
	<doc>The standard error output</doc>
**/
MAKE_STDIO(stderr);

DEFINE_PRIM(file_open,2);
DEFINE_PRIM(file_close,1);
DEFINE_PRIM(file_name,1);
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
