/* ************************************************************************ */
/*																			*/
/*  Neko ZLib Binding Library												*/
/*  Copyright (c)2006 Motion-Twin											*/
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
#include <stdlib.h>
#include <string.h>
#include <neko.h>
#include <zlib.h>

/**
	<doc>
	<h1>ZLib</h1>
	<p>
	Give access to the popular ZLib compression library, used in several file
	formats such as ZIP and PNG.
	</p>
	</doc>
**/

DEFINE_KIND(k_stream_def);
DEFINE_KIND(k_stream_inf);

#define val_stream(v)	((z_stream *)val_data(v))
#define val_flush(s)	*((int*)(((char*)s)+sizeof(z_stream)))

static field id_read, id_write, id_done;

DEFINE_ENTRY_POINT(zlib_main);

void zlib_main() {
	id_read = val_id("read");
	id_write = val_id("write");
	id_done = val_id("done");
}

static void free_stream_def( value v ) {
	z_stream *s = val_stream(v);
	deflateEnd(s); // no error
	free(s);
	val_kind(v) = NULL;
	val_gc(v,NULL);
}

static void free_stream_inf( value v ) {
	z_stream *s = val_stream(v);
	inflateEnd(s); // no error
	free(s);
	val_kind(v) = NULL;
	val_gc(v,NULL);
}

static void zlib_error( z_stream *z, int err ) {
	buffer b = alloc_buffer("ZLib Error : ");
	if( z && z->msg ) {
		buffer_append(b,z->msg);
		buffer_append(b," (");
	}
	val_buffer(b,alloc_int(err));
	if( z && z->msg )
		buffer_append_char(b,')');
	val_throw(buffer_to_string(b));
}

/**
	deflate_init : level:int -> 'dstream
	<doc>Open a compression stream with the given level of compression</doc>
**/
static value deflate_init( value level ) {
	z_stream *z;
	value s;
	int err;
	val_check(level,int);
	z = (z_stream*)malloc(sizeof(z_stream) + sizeof(int));
	memset(z,0,sizeof(z_stream));
	val_flush(z) = Z_NO_FLUSH;
	if( (err = deflateInit(z,val_int(level))) != Z_OK ) {
		free(z);
		zlib_error(NULL,err);
	}
	s = alloc_abstract(k_stream_def,z);
	val_gc(s,free_stream_def);
	return s;
}

/**
	deflate_buffer : 'dstream -> src:string -> srcpos:int -> dst:string -> dstpos:int -> { done => bool, read => int, write => int }
**/
static value deflate_buffer( value s, value src, value srcpos, value dst, value dstpos ) {
	z_stream *z;
	int slen, dlen, err;
	value o;
	val_check_kind(s,k_stream_def);
	val_check(src,string);
	val_check(srcpos,int);
	val_check(dst,string);
	val_check(dstpos,int);
	z = val_stream(s);
	if( val_int(srcpos) < 0 || val_int(dstpos) < 0 )
		neko_error();
	slen = val_strlen(src) - val_int(srcpos);
	dlen = val_strlen(dst) - val_int(dstpos);
	if( slen < 0 || dlen < 0 )
		neko_error();
	z->next_in = (Bytef*)(val_string(src) + val_int(srcpos));
	z->next_out = (Bytef*)(val_string(dst) + val_int(dstpos));
	z->avail_in = slen;
	z->avail_out = dlen;
	if( (err = deflate(z,val_flush(z))) < 0 )
		zlib_error(z,err);
	z->next_in = NULL;
	z->next_out = NULL;
	o = alloc_object(NULL);
	alloc_field(o,id_done,alloc_bool(err == Z_STREAM_END));
	alloc_field(o,id_read,alloc_int(slen - z->avail_in));
	alloc_field(o,id_write,alloc_int(dlen - z->avail_out));
	return o;
}

/**
	deflate_end : 'dstream -> void
	<doc>Close a compression stream</doc>
**/
static value deflate_end( value s ) {
	val_check_kind(s,k_stream_def);
	free_stream_def(s);
	return val_null;
}

/**
	inflate_init : window_size:int? -> 'istream
	<doc>Open a decompression stream</doc>
**/
static value inflate_init( value wsize ) {
	z_stream *z;
	value s;
	int err;
	int wbits;
	if( val_is_null(wsize) )
		wbits = MAX_WBITS;
	else {
		val_check(wsize,int);
		wbits = val_int(wsize);
	}
	z = (z_stream*)malloc(sizeof(z_stream) + sizeof(int));
	memset(z,0,sizeof(z_stream));
	val_flush(z) = Z_NO_FLUSH;
	if( (err = inflateInit2(z,wbits)) != Z_OK ) {
		free(z);
		zlib_error(NULL,err);
	}
	s = alloc_abstract(k_stream_inf,z);
	val_gc(s,free_stream_inf);
	return s;
}

/**
	inflate_buffer : 'istream -> src:string -> srcpos:int -> dst:string -> dstpos:int -> { done => bool, read => int, write => int }
**/
static value inflate_buffer( value s, value src, value srcpos, value dst, value dstpos ) {
	z_stream *z;
	int slen, dlen, err;
	value o;
	val_check_kind(s,k_stream_inf);
	val_check(src,string);
	val_check(srcpos,int);
	val_check(dst,string);
	val_check(dstpos,int);
	z = val_stream(s);
	if( val_int(srcpos) < 0 || val_int(dstpos) < 0 )
		neko_error();
	slen = val_strlen(src) - val_int(srcpos);
	dlen = val_strlen(dst) - val_int(dstpos);
	if( slen < 0 || dlen < 0 )
		neko_error();
	z->next_in = (Bytef*)(val_string(src) + val_int(srcpos));
	z->next_out = (Bytef*)(val_string(dst) + val_int(dstpos));
	z->avail_in = slen;
	z->avail_out = dlen;
	if( (err = inflate(z,val_flush(z))) < 0 )
		zlib_error(z,err);
	z->next_in = NULL;
	z->next_out = NULL;
	o = alloc_object(NULL);
	alloc_field(o,id_done,alloc_bool(err == Z_STREAM_END));
	alloc_field(o,id_read,alloc_int(slen - z->avail_in));
	alloc_field(o,id_write,alloc_int(dlen - z->avail_out));
	return o;
}

/**
	inflate_end : 'istream -> void
	<doc>Close a decompression stream</doc>
**/
static value inflate_end( value s ) {
	val_check_kind(s,k_stream_inf);
	free_stream_inf(s);
	return val_null;
}

/**
	set_flush_mode : 'stream -> string -> void
	<doc>Change the flush mode ("NO","SYNC","FULL","FINISH","BLOCK")</doc>
**/
static value set_flush_mode( value s, value flush ) {
	int f;
	if( !val_is_kind(s,k_stream_inf) )
		val_check_kind(s,k_stream_def);
	val_check(flush,string);
	if( strcmp(val_string(flush),"NO") == 0 )
		f = Z_NO_FLUSH;
	else if( strcmp(val_string(flush),"SYNC") == 0 )
		f = Z_SYNC_FLUSH;
	else if( strcmp(val_string(flush),"FULL") == 0 )
		f = Z_FULL_FLUSH;
	else if( strcmp(val_string(flush),"FINISH") == 0 )
		f = Z_FINISH;
	else if( strcmp(val_string(flush),"BLOCK") == 0 )
		f = Z_BLOCK;
	else
		neko_error();
	val_flush(val_stream(s)) = f;
	return val_null;
}

/**
	get_adler32 : 'stream -> 'int32
	<doc>Returns the adler32 value of the stream</doc>
**/
static value get_adler32( value s ) {
	if( !val_is_kind(s,k_stream_inf) )
		val_check_kind(s,k_stream_def);
	return alloc_int32(val_stream(s)->adler);
}


/**
	update_adler32 : adler:'int32 -> string -> pos:int -> len:int -> 'int32
	<doc>Update an adler32 value with a substring</doc>
**/
static value update_adler32( value adler, value s, value pos, value len ) {
	val_check(adler,int32);
	val_check(s,string);
	val_check(pos,int);
	val_check(len,int);
	if( val_int(pos) < 0 || val_int(len) < 0 || val_int(pos) + val_int(len) > val_strlen(s) )
		neko_error();
	return alloc_int32(adler32(val_int32(adler),(Bytef*)(val_string(s)+val_int(pos)),val_int(len)));
}

/**
	update_crc32 : crc:'int32 -> string -> pos:int -> len:int -> 'int32
	<doc>Update a CRC32 value with a substring</doc>
**/
static value update_crc32( value crc, value s, value pos, value len ) {
	val_check(crc,int32);
	val_check(s,string);
	val_check(pos,int);
	val_check(len,int);
	if( val_int(pos) < 0 || val_int(len) < 0 || val_int(pos) + val_int(len) > val_strlen(s) )
		neko_error();
	return alloc_int32(crc32(val_int32(crc),(Bytef*)(val_string(s)+val_int(pos)),val_int(len)));
}

/**
	deflate_bound : 'dstream -> n:int -> int
	<doc>Return the maximum buffer size needed to write [n] bytes</doc>
**/
static value deflate_bound( value s, value size ) {
	val_check_kind(s,k_stream_def);
	val_check(size,int);
	return alloc_int(deflateBound(val_stream(s),val_int(size)));
}

DEFINE_PRIM(deflate_init,1);
DEFINE_PRIM(deflate_buffer,5);
DEFINE_PRIM(deflate_end,1);
DEFINE_PRIM(inflate_init,1);
DEFINE_PRIM(inflate_buffer,5);
DEFINE_PRIM(inflate_end,1);
DEFINE_PRIM(set_flush_mode,2);
DEFINE_PRIM(deflate_bound,2);

DEFINE_PRIM(get_adler32,1);
DEFINE_PRIM(update_adler32,4);
DEFINE_PRIM(update_crc32,4);

/* ************************************************************************ */
