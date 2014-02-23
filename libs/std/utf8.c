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
#include <string.h>


/**
	<doc>
	<h1>UTF8</h1>
	<p>
	Operations on UTF8 strings. 
	Most of the operations are optimized for speed so they might still 
	succeed on some malformed UTF8 string. The only function that completely
	check the UTF8 format is [utf8_validate]. Other functions might raise
	some exception or not depending on the malformed data.
	</p>
	</doc>
**/


typedef struct {
	value buf;
	int pos;
	int nesc;
} ubuf;

DEFINE_KIND(k_ubuf);

/**
	utf8_buf_alloc : size:int -> 'ubuf
	<doc>Create a new buffer with an initial size in bytes</doc>
**/
static value utf8_buf_alloc( value size ) {
	ubuf *b;
	val_check(size,int);
	if( val_int(size) < 0 )
		neko_error();
	b = (ubuf*)alloc(sizeof(ubuf));
	b->buf = alloc_empty_string(val_int(size));
	b->nesc = 0;
	b->pos = 0;
	return alloc_abstract(k_ubuf,b);
}

static void utf8_buf_resize( ubuf *b ) {
	value s;
	int len = val_strlen(b->buf);
	// allocate a number of bytes depending on previously
	// escaped caracters, with a minimum of 10
	int nbytes = (b->nesc + b->pos * 2 - 1) / (b->pos?b->pos:1);
	if( nbytes < 10 )
		nbytes = 10;
	s = alloc_empty_string(len+nbytes);
	memcpy(val_string(s),val_string(b->buf),len);
	b->buf = s;	
}


/**
	utf8_buf_add : 'buf -> int -> void
	<doc>Add a valid UTF8 char (0 - 0x10FFFF) to the buffer</doc>
**/
static value utf8_buf_add( value buf, value uchar ) {
	ubuf *b;
	unsigned char *s;
	unsigned int c;
	val_check_kind(buf,k_ubuf);
	val_check(uchar,int);
	b = (ubuf*)val_data(buf);
	c = (unsigned)val_int(uchar);
	if( c <= 0x7F ) {
		if( b->pos >= val_strlen(b->buf) )
			utf8_buf_resize(b);
		val_string(b->buf)[b->pos++] = (char)c;
		return val_true;
	}	
	if( b->pos + 4 > val_strlen(b->buf) )
		utf8_buf_resize(b);
	s = (unsigned char*)val_string(b->buf);
	if( c <= 0x7FF ) {
		b->nesc += 1;
		s[b->pos++] = 0xC0 | (c >> 6);
		s[b->pos++] = 0x80 | (c & 63);
	} else if( c <= 0xFFFF ) {
		b->nesc += 2;
		s[b->pos++] = 0xE0 | (c >> 12);
		s[b->pos++] = 0x80 | ((c >> 6) & 63);
		s[b->pos++] = 0x80 | (c & 63);
	} else if( c <= 0x10FFFF ) {
		b->nesc += 3;
		s[b->pos++] = 0xF0 | (c >> 18);
		s[b->pos++] = 0x80 | ((c >> 12) & 63);
		s[b->pos++] = 0x80 | ((c >> 6) & 63);
		s[b->pos++] = 0x80 | (c & 63);
	} else
		neko_error();
	return val_true;
}

/**
	utf8_buf_content : 'buf -> string
	<doc>
	Return the current content of the buffer.
	This is not a copy of the buffer but the shared content.
	Retreiving content and then continuing to add chars is 
	possible but not very efficient.
	</doc>
**/
static value utf8_buf_content( value buf ) {
	ubuf *b;
	val_check_kind(buf,k_ubuf);
	b = (ubuf*)val_data(buf);
	val_set_length(b->buf,b->pos);
	val_string(b->buf)[b->pos] = 0;
	return b->buf;
}

/**
	utf8_buf_length : 'buf -> int
	<doc>Return the number of UTF8 chars stored in the buffer</doc>
**/
static value utf8_buf_length( value buf ) {
	ubuf *b;
	val_check_kind(buf,k_ubuf);
	b = (ubuf*)val_data(buf);
	return alloc_int(b->pos - b->nesc);
}

/**
	utf8_buf_size : 'buf -> int
	<doc>Return the current size in bytes of the buffer</doc>
**/
static value utf8_buf_size( value buf ) {
	ubuf *b;
	val_check_kind(buf,k_ubuf);
	b = (ubuf*)val_data(buf);
	return alloc_int(b->pos);
}

/**
	utf8_validate : string -> bool
	<doc>Validate if a string is encoded using the UTF8 format</doc>
**/
static value utf8_validate( value str ) {
	int l;
	unsigned char *s;
	val_check(str,string);
	l = val_strlen(str);
	s = (unsigned char*)val_string(str);
	while( l-- ) {
		unsigned char c = *s++;
		if( c < 0x7F )
			continue;
		else if( c < 0xC0 )
			return val_false;
		else if( c < 0xE0 ) {
			if( (*s++ & 0x80) != 0x80 )
				return val_false;
			l--;
		} else if( c < 0xF0 ) {
			if( (*s++ & 0x80) != 0x80 )
				return val_false;
			if( (*s++ & 0x80) != 0x80 )
				return val_false;
			l-=2;
		} else {
			if( (*s++ & 0x80) != 0x80 )
				return val_false;
			if( (*s++ & 0x80) != 0x80 )
				return val_false;
			if( (*s++ & 0x80) != 0x80 )
				return val_false;
			l-=3;
		}
	}
	return val_true;
}

/**
	utf8_length : string -> int
	<doc>Returns the number of UTF8 chars in the string.</doc>
**/
static value utf8_length( value str ) {
	int l;
	int count = 0;
	unsigned char *s;
	val_check(str,string);
	l = val_strlen(str);
	s = (unsigned char*)val_string(str);
	while( l > 0 ) {
		unsigned char c = *s;
		count++;
		if( c < 0x7F ) {
			l--;
			s++;
		} else if( c < 0xC0 )
			neko_error();
		else if( c < 0xE0 ) {
			l-=2;
			s+=2;
		} else if( c < 0xF0 ) {
			l-=3;
			s+=3;
		} else {
			l-=4;
			s+=4;
		}
	}
	if( l < 0 )
		neko_error();
	return alloc_int(count);
}

/**
	utf8_sub : string -> pos:int -> len:int -> string
	<doc>Returns a part of an UTF8 string.</doc>
**/
static value utf8_sub( value str, value pos, value len ) {
	int l;
	int count;
	unsigned char *s, *save;
	val_check(str,string);
	val_check(pos,int);
	val_check(len,int);
	l = val_strlen(str);
	count = val_int(pos);
	if( count < 0 )
		neko_error();
	s = (unsigned char*)val_string(str);
	while( count-- && l > 0 ) {
		unsigned char c = *s;
		if( c < 0x7F ) {
			l--;
			s++;
		} else if( c < 0xC0 )
			neko_error();
		else if( c < 0xE0 ) {
			l-=2;
			s+=2;
		} else if( c < 0xF0 ) {
			l-=3;
			s+=3;
		} else {
			l-=4;
			s+=4;
		}
	}
	if( l < 0 )
		neko_error();
	save = s;
	count = val_int(len);
	if( count < 0 )
		neko_error();
	while( count-- && l > 0 ) {
		unsigned char c = *s;
		if( c < 0x7F ) {
			l--;
			s++;
		} else if( c < 0xC0 )
			neko_error();
		else if( c < 0xE0 ) {
			l-=2;
			s+=2;
		} else if( c < 0xF0 ) {
			l-=3;
			s+=3;
		} else {
			l-=4;
			s+=4;
		}
	}
	if( l < 0 )
		neko_error();
	l = (int)(s - save);
	str = alloc_empty_string(l);
	memcpy(val_string(str),save,l);
	return str;
}

/**
	utf8_get : string -> n:int -> int
	<doc>Returns the [n]th char in an UTF8 string. 
	This might be inefficient if [n] is big.</doc>
**/
static value utf8_get( value str, value pos ) {
	int l;
	int p;
	unsigned char *s;
	val_check(pos,int);
	val_check(str,string);
	l = val_strlen(str);
	p = val_int(pos);
	if( p < 0 )
		neko_error();
	s = (unsigned char*)val_string(str);
	while( l-- ) {
		unsigned char c = *s++;
		if( c < 0x7F ) {
			if( p-- == 0 )
				return alloc_int(c);
		} else if( c < 0xC0 )
			neko_error();
		else if( c < 0xE0 ) {
			l--;
			if( l < 0 )
				neko_error();
			if( p-- == 0 )
				return alloc_int(((c & 0x3F) << 6) | ((*s) & 0x7F));
			s++;
		} else if( c < 0xF0 ) {
			l -= 2;
			if( l < 0 )
				neko_error();
			if( p-- == 0 )
				return alloc_int(((c & 0x1F) << 12) | (((*s) & 0x7F) << 6) | (s[1] & 0x7F));
			s += 2;
		} else {
			l -= 3;
			if( l < 0 )
				neko_error();
			if( p-- == 0 )
				return alloc_int(((c & 0x0F) << 18) | (((*s) & 0x7F) << 12) | ((s[1] & 0x7F) << 6) | (s[2] & 0x7F));
			s += 3;
		}
	}
	neko_error();
	return val_true;
}

/**
	utf8_iter : string -> f:(int -> void) -> void
	<doc>Call [f] with each of UTF8 char of the string.</doc>
**/
static value utf8_iter( value str, value f ) {
	int l;
	unsigned char *s;
	val_check(str,string);
	val_check_function(f,1);
	l = val_strlen(str);
	s = (unsigned char*)val_string(str);
	while( l-- ) {
		unsigned char c = *s++;
		if( c < 0x7F )
			val_call1(f,alloc_int(c));
		else if( c < 0xC0 )
			neko_error();
		else if( c < 0xE0 ) {
			l--;
			if( l < 0 )
				neko_error();
			val_call1(f,alloc_int(((c & 0x3F) << 6) | ((*s) & 0x7F)));
			s++;
		} else if( c < 0xF0 ) {
			l -= 2;
			if( l < 0 )
				neko_error();
			val_call1(f,alloc_int(((c & 0x1F) << 12) | (((*s) & 0x7F) << 6) | (s[1] & 0x7F)));
			s += 2;
		} else {
			l -= 3;
			if( l < 0 )
				neko_error();
			val_call1(f,alloc_int(((c & 0x0F) << 18) | (((*s) & 0x7F) << 12) | ((s[1] & 0x7F) << 6) | (s[2] & 0x7F)));
			s += 3;
		}
	}
	return val_true;
}

/**
	utf8_compare : s1:string -> s2:string -> int
	<doc>Compare two UTF8 strings according to UTF8 char codes.</doc>
**/
static value utf8_compare( value str1, value str2 ) {
	int l1, l2, l;
	unsigned char *s1, *s2;
	val_check(str1,string);
	val_check(str2,string);
	l1 = val_strlen(str1);
	l2 = val_strlen(str2);
	s1 = (unsigned char*)val_string(str1);
	s2 = (unsigned char*)val_string(str2);
	l = (l1 < l2)?l1:l2;
	while( l-- ) {
		unsigned char c1 = *s1++;
		unsigned char c2 = *s2++;
		if( c1 != c2 )
			return alloc_int((c1 > c2)?-1:1);
		if( c1 < 0x7F )
			continue;
		else if( c1 < 0xC0 )
			neko_error();
		else if( c1 < 0xE0 ) {
			l--;
			if( l < 0 )
				neko_error();
			if( *s1++ != *s2++ )
				return alloc_int((s1[-1] > s2[-1])?-1:1);
		} else if( c1 < 0xF0 ) {
			l-=2;
			if( l < 0 )
				neko_error();
			if( *s1++ != *s2++ )
				return alloc_int((s1[-1] > s2[-1])?-1:1);
			if( *s1++ != *s2++ )
				return alloc_int((s1[-1] > s2[-1])?-1:1);
		} else {
			l -= 3;
			if( l < 0 )
				neko_error();
			if( *s1++ != *s2++ )
				return alloc_int((s1[-1] > s2[-1])?-1:1);
			if( *s1++ != *s2++ )
				return alloc_int((s1[-1] > s2[-1])?-1:1);
			if( *s1++ != *s2++ )
				return alloc_int((s1[-1] > s2[-1])?-1:1);
		}
	}
	if( l1 != l2 )
		return alloc_int((l1 > l2)?1:-1);
	return alloc_int(0);
}

DEFINE_PRIM(utf8_buf_alloc,1);
DEFINE_PRIM(utf8_buf_add,2);
DEFINE_PRIM(utf8_buf_content,1);
DEFINE_PRIM(utf8_buf_length,1);
DEFINE_PRIM(utf8_buf_size,1);

DEFINE_PRIM(utf8_get,2);
DEFINE_PRIM(utf8_validate,1);
DEFINE_PRIM(utf8_iter,2);
DEFINE_PRIM(utf8_length,1);
DEFINE_PRIM(utf8_compare,2);
DEFINE_PRIM(utf8_sub,3);

/* ************************************************************************ */
