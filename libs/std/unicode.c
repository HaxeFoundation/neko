/*
 * Copyright (C)2005-2014 Haxe Foundation
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
	<h1>Unicode</h1>
	<p>
	Operations on Unicode strings. 
	Most of the operations are optimized for speed so they might still 
	succeed on some malformed Unicode string. The only function that completely
	check the Unicode format is [unicode_validate]. Other functions might raise
	some exception or not depending on the malformed data.
	</p>
	<p>
	Supported encodings are "ascii", "iso-latin1", "utf8", "ucs2-le", "ucs2-be", "utf16-le", "utf16-be", "utf32-le", "utf32-be"
	</p>
	</doc>
**/

#define INVALID_CHAR 0xFFFFFFFF
typedef unsigned int uchar;
typedef unsigned char *ustring;

typedef enum {
	ASCII = 0,
	ISO_LATIN1 = 1,
	UTF8 = 2,
	UCS2_LE	= 3,
	UCS2_BE	= 4,
	UTF16_LE = 5,
	UTF16_BE = 6,
	UTF32_LE = 7,
	UTF32_BE = 8,
	LAST_ENCODING = 9
} encoding;

static const char *encodings[] = {
	"ascii",
	"iso-latin1",
	"utf8",
	"ucs2-le",
	"ucs2-be",
	"utf16-le",
	"utf16-be",
	"utf32-le",
	"utf32-be",
};

DEFINE_KIND(k_uni_buf);

static void TODO() {
	val_throw(alloc_string("Not implemented"));
}

/* ------------ UCHAR LIB FUNCTIONS -------------------- */

static int uchar_size( uchar c, encoding e ) {
	switch( e ) {
	case ASCII:
		if( c >= 0x80 ) return 0;
		return 1;
	case ISO_LATIN1:
		if( c >= 0x100 ) return 0;
		return 1;
	case UCS2_LE, UCS2_BE:
		if( c >= 0x10000 ) return 0;
		return 2;
	case UTF16_LE, UTF16_BE:
		return c >= 0x10000 ? 4 : 2;
	case UTF32_LE, UTF32_BE:
		return 4;
	case UTF8:
		if( c < 0x7F ) return 1;
		if( c < 0xC0 ) return 0;
		if( c < 0xE0 ) return 2;
		if( c < 0xF0 ) return 3;
		return 4;
	default:
		return 0;
	}
}

static int uchar_set( ustring str, encoding e, uchar c ) {
	TODO();
}

static uchar uchar_get( ustring str, int size, encoding e, int pos ) {
	if( pos < 0 ) return INVALID_CHAR;
	switch( e ) {
	case ASCII, ISO_LATIN1:
		if( pos >= size ) return INVALID_CHAR:
		return str[pos];
	case UCS2_LE, UCS2_BE:
		
	}
}

static ustring uchar_pos( ustring str, int size, encoding e, int pos ) {
	if( pos < 0 )
		return NULL;
	switch( e ) {
	case ASCII, ISO_LATIN1:
		if( pos > size ) return NULL;
		return str + pos;
	case UCS2_LE, UCS2_BE:
		if( pos > (size >> 1) ) return NULL;
		return str + (pos << 1);
	case UCS32_LE, UCS32_BE:
		if( pos > (size >> 2) ) return NULL;
		return str + (pos << 2);
	case UTF16_LE, UTF16_BE:
		TODO();
		break;
	case UTF8:
		while( pos-- && size > 0 ) {
			unsigned char c = *str;
			if( c < 0x7F ) {
				size--;
				str++;
			} else if( c < 0xC0 )
				return NULL;
			else if( c < 0xE0 ) {
				size-=2;
				str+=2;
			} else if( c < 0xF0 ) {
				size-=3;
				str+=3;
			} else {
				size-=4;
				str+=4;
			}
		}
		if( size < 0 )
			return NULL;
		return str;
	default:
		TODO();
	}
	return NULL;
}

/* -------------------------------------------------------------------- */

/**
	unicode_encoding : string -> int
	<doc>Gets the encoding code corresponding the given value
		ascii=0
		iso-latin1=1
		utf8=2
		ucs2-le=3
		ucs2-be=4
		utf16-le=5
		utf16-be=6
		utf32-le=7
		utf32-be=8
	</doc>
**/
static value unicode_encoding_code( value str ) {
	int i;
	val_check(str, string);
	for( i = 0; i < LAST_ENCODING; i++ )
		if( strcmp(val_string(str), encodings[i]) == 0 )
			return alloc_int(i);
	neko_error();
	return val_null;
}

static value unicode_encoding_string( value enc ) {
	val_check(enc, int);
	if( val_int(enc) < 0 || val_int(enc) >= LAST_ENCODING )
		neko_error();
	return alloc_string(encodings[val_int(enc)]);
}

typedef struct {
	value buf;
	encoding enc;
	int pos;
	int nchars;
} uni_buf;

static encoding get_encoding( value v ) {
	int e;
	if( !val_is_int(v) || (e = val_int(v)) < 0 || e >= LAST_ENCODING )
		val_throw(alloc_string("Invalid encoding value"));
	return e;
}

/**
	unicode_buf_alloc : size:int -> encoding:int -> 'ubuf
	<doc>Create a new buffer with an initial size in bytes and specific encoding.</doc>
**/
static value unicode_buf_alloc( value size, value encoding ) {
	uni_buf *b;
	int enc;
	val_check(size,int);
	if( val_int(size) < 0 )
		neko_error();
	b = (uni_buf*)alloc(sizeof(uni_buf));
	b->buf = alloc_empty_string(val_int(size));
	b->pos = 0;
	b->nchars = 0;
	b->encoding = get_encoding(encoding);
	return alloc_abstract(k_uni_buf,b);
}

static void unicode_buf_resize( uni_buf *b ) {
	value s;
	int len = val_strlen(b->buf);
	int size2 = (len * 3) >> 1;
	if( size2 - len < 10 ) size2 = 10 + len;
	s = alloc_empty_string(size2);
	memcpy(val_string(s),val_string(b->buf),len);
	b->buf = s;	
}


/**
	unicode_buf_add : 'buf -> int -> void
	<doc>Add an Unicode char to the buffer</doc>
**/
static value unicode_buf_add( value buf, value uchar ) {
	uni_buf *b;
	unsigned char *s;
	uchar c;
	int req;
	val_check_kind(buf,k_uni_buf);
	val_check(uchar,int);
	b = (uni_buf*)val_data(buf);
	c = (uchar)val_int(uchar);
	req = uchar_size(c, b->encoding);
	if( req == 0 )
		neko_error();
	if( b->pos + req > val_strlen(b->buf) )
		unicode_buf_resize(b);
	s = (unsigned char*)val_string(b->buf);
	uchar_set(c, s, b->encoding);
	return val_null;
}

/**
	unicode_buf_content : 'buf -> string
	<doc>
	Return the current content of the buffer.
	This is not a copy of the buffer but the shared content.
	Retreiving content and then continuing to add chars is 
	possible but not very efficient.
	</doc>
**/
static value unicode_buf_content( value buf ) {
	uni_buf *b;
	val_check_kind(buf,k_uni_buf);
	b = (uni_buf*)val_data(buf);
	val_set_length(b->buf,b->pos);
	val_string(b->buf)[b->pos] = 0;
	return b->buf;
}

/**
	unicode_buf_length : 'buf -> int
	<doc>Return the number of Unicode chars stored in the buffer</doc>
**/
static value unicode_buf_length( value buf ) {
	uni_buf *b;
	val_check_kind(buf,k_uni_buf);
	b = (uni_buf*)val_data(buf);
	return alloc_int(b->nchars);
}

/**
	unicode_buf_size : 'buf -> int
	<doc>Return the current size in bytes of the buffer</doc>
**/
static value unicode_buf_size( value buf ) {
	uni_buf *b;
	val_check_kind(buf,k_uni_buf);
	b = (uni_buf*)val_data(buf);
	return alloc_int(b->pos);
}

/**
	unicode_validate : s:string -> encoding:int -> bool
	<doc>Tells if a string [s] is encoded using the given Unicode format.</doc>
**/
static value unicode_validate( value str, value encoding ) {
	int l;
	encoding enc;
	unsigned char *s;
	val_check(str,string);
	l = val_strlen(str);
	s = (unsigned char*)val_string(str);
	switch( get_encoding(encoding) ) {
	case ASCII:
		while( l-- ) {
			unsigned char c = *s++;
			if( c >= 0x80 )
				return val_false;
		}
		break;
	case ISO_LATIN1:
		return val_true;
	case UCS2_LE:
		if( s[0] == 0xFE && s[1] == 0xFF ) return val_false; // BOM fail
		return alloc_bool(l & 1 == 0);
	case UCS2_BE:
		if( s[0] == 0xFF && s[1] == 0xFE ) return val_false; // BOM fail
		return alloc_bool(l & 1 == 0);
	case UTF32_LE:
		if( l >= 4 && s[0] == 0 && s[1] == 0 && s[2] == 0xFE && s[3] == 0xFF ) return val_false; // BOM fail
		return alloc_bool(l & 3 == 0);	
	case UTF32_BE:
		if( l >= 4 && s[0] == 0xFF && s[1] == 0xFE && s[2] == 0 && s[3] == 0 ) return val_false; // BOM fail
		return alloc_bool(l & 3 == 0);	
	case UTF16_LE:
		if( s[0] == 0xFE && s[1] == 0xFF ) return val_false; // BOM fail
		// TODO : validate surrogate pairs
		return alloc_bool(l & 1 == 0);
	case UTF16_BE:
		if( s[0] == 0xFF && s[1] == 0xFE ) return val_false; // BOM fail
		// TODO : validate surrogate pairs
		return alloc_bool(l & 1 == 0);
	case UTF8:
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
	default:
		TODO();
		break;
	}
	return val_false;
}

/**
	unicode_length : string -> encoding:int -> int
	<doc>Returns the number of Unicode chars in the string.</doc>
**/
static value unicode_length( value str, value encoding ) {
	int l;
	int count = 0;
	ustring s;
	val_check(str,string);
	l = val_strlen(str);
	s = (ustring)val_string(str);
	switch( get_encoding(encoding) ) {
	case ASCII, ISO_LATIN1:
		count = l;
		break;
	case UCS2_LE, UCS2_BE:
		count = l >> 1;
		break;
	case UTF16_LE, UTF16_BE:
		TODO();
		break;
	case UTF32_LE, UTF32_BE:
		count = l >> 2;
		break;
	case UTF8:
		ustring end = s + l;
		while( s < end ) {
			unsigned char c = *s;
			count++;
			if( c < 0x7F )
				s++;
			else if( c < 0xC0 )
				neko_error();
			else if( c < 0xE0 )
				s+=2;
			else if( c < 0xF0 )
				s+=3;
			else
				s+=4;
		}
		if( s > end )
			neko_error();
		break;
	}
	return alloc_int(count);
}

/**
	unicode_sub : string -> encoding:int -> pos:int -> len:int -> string
	<doc>Returns a part of an Unicode string.</doc>
**/
static value unicode_sub( value str, value enc, value vpos, value vlen ) {
	int l;
	int pos, len;
	encoding e;
	ustring s, start, end;
	val_check(str,string);
	val_check(vpos,int);
	val_check(vlen,int);
	e = get_encoding(enc);
	l = val_strlen(str);
	pos = val_int(vpos);
	len = val_int(vlen);
	s = (ustring)val_string(str);
	start = uchar_pos(s,l,e,pos);
	if( start == NULL )
		neko_error();
	end = uchar_pos(start,l-(start-s),e,len);
	if( end == NULL )
		neko_error();
	l = (int)(end - start);
	str = alloc_empty_string(l);
	memcpy(val_string(str),start,l);
	return str;
}

/**
	unicode_get : string -> encoding:int -> n:int -> int
	<doc>Returns the [n]th char in an Unicode string. 
	This might be inefficient if [n] is big and the string has variable length per char.</doc>
**/
static value unicode_get( value str, value enc, value pos ) {
	int p;
	uchar c;
	encoding e;
	ustring s;
	val_check(str,string);
	val_check(pos,int);
	c = uchar_get((ustring)val_string(str), val_strlen(str), get_encoding(e), val_int(pos));
	if( c == INVALID_CHAR ) neko_error();
	return alloc_best_int(c);
}

/**
	unicode_iter : string -> encoding:int -> f:(int -> void) -> void
	<doc>Call [f] with each of Unicode char of the string.</doc>
**/
static value unicode_iter( value str, value encoding, value f ) {
	int l;
	encoding e;
	ustring s, end;
	val_check(str,string);
	val_check_function(f,1);
	e = get_encoding(encoding);
	s = (ustring)val_string(str);
	end = s + val_strlen(str);
	while( s < end ) {
		uchar c = uchar_get(s, end - s, e, 0);
		s = uchar_pos(s, end - s, e, 1);
		val_call1(f,alloc_best_int(c));
	}
	return val_null;
}

/**
	unicode_compare : s1:string -> s2:string -> encoding:int -> int
	<doc>Compare two Unicode strings according to their char codes.</doc>
**/
static value unicode_compare( value str1, value str2, value encoding ) {
	int l1, l2, l;
	encoding e;
	ustring s1, s2;
	val_check(str1,string);
	val_check(str2,string);
	l1 = val_strlen(str1);
	l2 = val_strlen(str2);
	s1 = (ustring)val_string(str1);
	s2 = (ustring)val_string(str2);
	l = (l1 < l2)?l1:l2;
	switch( get_encoding(e) ) {
	case ISO_LATIN1, ASCII, UCS2_BE, UTF16_BE, UTF32_BE:
		int r = memcmp(s1,s2,l);
		if( r != 0 )
			return alloc_int(r);
		break;
	case UCS2_LE:
		TODO();
		break;
	case UTF16_LE:
		TODO();
		break;
	case UTF32_LE:
		TODO();
		break;
	case UTF8:
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
	}
	if( l1 != l2 )
		return alloc_int((l1 > l2)?1:-1);
	return alloc_int(0);
}

/**
	unicode_convert : s1:string -> encoding:int -> to_encoding:int -> string
	<doc>Comvert an Unicode string from a given encoding to another.</doc>
**/
static value unicode_convert( value str1, value str2, value encoding ) {
	TODO();
	return val_null;
}

DEFINE_PRIM(unicode_buf_alloc,2);
DEFINE_PRIM(unicode_buf_add,2);
DEFINE_PRIM(unicode_buf_content,1);
DEFINE_PRIM(unicode_buf_length,1);
DEFINE_PRIM(unicode_buf_size,1);

DEFINE_PRIM(unicode_get,3);
DEFINE_PRIM(unicode_validate,2);
DEFINE_PRIM(unicode_iter,3);
DEFINE_PRIM(unicode_length,2);
DEFINE_PRIM(unicode_compare,3);
DEFINE_PRIM(unicode_sub,4);

DEFINE_PRIM(unicode_convert,3);

/* ************************************************************************ */
