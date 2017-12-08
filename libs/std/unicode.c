/*
 * Copyright (C)2005-2017 Haxe Foundation
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
#include <neko_vm.h>
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

#include "unicase.c"

#define val_ustring(s) ((ustring)val_string(s))

typedef enum {
	ASCII = 0,
	ISO_LATIN1 = 1,
	UTF8 = 2,
	UCS2_BE	= 3,
	UCS2_LE	= 4,
	UTF16_BE = 5,
	UTF16_LE = 6,
	UTF32_BE = 7,
	UTF32_LE = 8,
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

static int utf8_codelen[16] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0,
	2, 2, 3, 4
};

DEFINE_KIND(k_uni_buf);

#define IS_BE(e) (neko_is_big_endian() ^ ((e) & 1))

#define u16be(v) (((v) >> 8) | (((v) << 8) & 0xFF00))
#define u32be(v) (((v) >> 24) | (((v) >> 8) & 0xFF00) | (((v) << 8) & 0xFF0000) | ((v) << 24))

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
	case UCS2_LE:
	case UCS2_BE:
		if( c >= 0x10000 ) return 0;
		return 2;
	case UTF16_LE:
	case UTF16_BE:
		if( c >= 0x110000 ) return 0;
		return c >= 0x10000 ? 4 : 2;
	case UTF32_LE:
	case UTF32_BE:
		return 4;
	case UTF8:
		if( c >= 0x200000 ) return 0;
		return utf8_codelen[c>>4];
	default:
		TODO();
		break;
	}
	return 0;
}

static void uchar_set( ustring str, encoding e, uchar c ) {
	switch( e ) {
	case ASCII:
	case ISO_LATIN1:
		*str = c;
		break;
	case UTF8:
		if( c < 0x80 )
			*str++ = c;
		else if( c < 0x800 ) {
			*str++ = (c >> 6);
			*str++ = (c & 63);
		} else if( c < 0x10000 ) {
			*str++ = 0xE0 | (c >> 12);
			*str++ = 0x80 | ((c >> 6) & 63);
			*str++ = 0x80 | (c & 63);
		} else {
			*str++ = 0xF0 | (c >> 18);
			*str++ = 0x80 | ((c >> 12) & 63);
			*str++ = 0x80 | ((c >> 6) & 63);
			*str++ = 0x80 | (c & 63);
		}
		break;
	case UCS2_LE:
	case UCS2_BE:
		if( IS_BE(e) )
			c = u16be(c);
		*((unsigned short*)str) = c;
		break;
	case UTF32_LE:
	case UTF32_BE:
		if( IS_BE(e) )
			c = u32be(c);
		*((unsigned int*)str) = c;
		break;
	}
}

static ustring uchar_pos( ustring str, int size, encoding e, int pos ) {
	if( pos < 0 )
		return NULL;
	switch( e ) {
	case ASCII:
	case ISO_LATIN1:
		if( pos > size ) return NULL;
		return str + pos;
	case UCS2_LE:
	case UCS2_BE:
		if( pos > (size >> 1) ) return NULL;
		return str + (pos << 1);
	case UTF32_LE:
	case UTF32_BE:
		if( pos > (size >> 2) ) return NULL;
		return str + (pos << 2);
	case UTF16_LE:
	case UTF16_BE: {
		ustring end = str + size;
		int dec = IS_BE(e) ? 0 : 8;
		while( str + 1 < end && pos-- > 0 ) {
			unsigned short c = *(unsigned short *)str;
			if( ((c >> dec) & 0xFC) == 0xD8 )
				str += 4;
			else
				str += 2;
		}
		if( str > end )
			return NULL;
		return str;
		}
	case UTF8: {
		ustring end = str + size;
		while( str < end && pos-- > 0 ) {
			int l = utf8_codelen[(*str)>>4];
			if( l == 0 ) return NULL;
			str += l;
		}
		if( str > end )
			return NULL;
		return str;
		}
	default:
		TODO();
		break;
	}
	return NULL;
}

static uchar uchar_get( ustring *rstr, int size, encoding e, int pos ) {
	uchar c;
	ustring str = *rstr;
	if( pos < 0 ) return INVALID_CHAR;
	switch( e ) {
	case ASCII:
	case ISO_LATIN1:
		if( pos >= size ) return INVALID_CHAR;
		str = str + pos;
		c = *str++;
		break;
	case UCS2_LE:
	case UCS2_BE:
		if( pos >= size << 1 ) return INVALID_CHAR;
		str = str + (pos<<1);
		c = *((unsigned short *)str);
		str += 2;
		if( IS_BE(e) )
			c = u16be(c);
		break;
	case UTF32_LE:
	case UTF32_BE:
		if( pos >= size << 2 ) return INVALID_CHAR;
		str = str + (pos<<2);
		c = *((unsigned int *)str);
		str += 4;
		if( IS_BE(e) )
			c = u32be(c);
		break;
	case UTF16_LE:
	case UTF16_BE:
		if( pos > 0 ) {
			ustring str2 = uchar_pos(str, size, e, pos);
			size = (str + size) - str2;
			str = str2;
		}
		if( size < 2 ) return INVALID_CHAR;
		c = *(unsigned short *)str;
		str += 2;
		if( IS_BE(e) )
			c = u16be(c);
		if( (c & 0xFC00) == 0xD800 ) {
			unsigned short c2;
			if( size < 4 ) return INVALID_CHAR;
			c2 = *(unsigned short *)str;
			str += 2;
			if( IS_BE(e) )
				c2 = u16be(c2);
			if( (c2 & 0xFC00) != 0xDC00 )
				return INVALID_CHAR;
			c = (((c&0x3FFF)<<10) | (c2&0x3FFF)) + 0x10000;
		}
		break;
	case UTF8:
		if( pos > 0 ) {
			ustring str2 = uchar_pos(str, size, e, pos);
			size = (str + size) - str2;
			str = str2;
		}
		if( size < 1 ) return INVALID_CHAR;
		c = *str;
		if( c >= 0x80 ) {
			int len = utf8_codelen[c>>4];
			if( len == 0 || size < len ) return INVALID_CHAR;
			if( c < 0xE0 ) {
				c = ((c & 0x3F) << 6) | ((*str) & 0x7F);
				str++;
			} else if( c < 0xF0 ) {
				c = ((c & 0x1F) << 12) | (((*str) & 0x7F) << 6) | (str[1] & 0x7F);
				str += 2;
			} else {
				c = ((c & 0x0F) << 18) | (((*str) & 0x7F) << 12) | ((str[1] & 0x7F) << 6) | (str[2] & 0x7F);
				str += 3;
			}
		}
		break;
	default:
		c = INVALID_CHAR;
		TODO();
		break;
	}
	*rstr = str;
	return c;
}

/* -------------------------------------------------------------------- */

/**
	unicode_encoding : string -> int
	<doc>Gets the encoding code corresponding the given value
		ascii=0
		iso-latin1=1
		utf8=2
		ucs2-be=3
		ucs2-le=4
		utf16-be=5
		utf16-le=6
		utf32-be=7
		utf32-le=8
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
	val_check(size,int);
	if( val_int(size) < 0 )
		neko_error();
	b = (uni_buf*)alloc(sizeof(uni_buf));
	b->buf = alloc_empty_string(val_int(size));
	b->pos = 0;
	b->nchars = 0;
	b->enc = get_encoding(encoding);
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
static value unicode_buf_add( value buf, value uc ) {
	uni_buf *b;
	uchar c;
	int req;
	val_check_kind(buf,k_uni_buf);
	val_check(uc,int);
	b = (uni_buf*)val_data(buf);
	c = (uchar)val_int(uc);
	req = uchar_size(c, b->enc);
	if( req == 0 )
		val_throw(alloc_string("Unicode char outside of encoding range"));
	if( b->pos + req > val_strlen(b->buf) )
		unicode_buf_resize(b);
	uchar_set(val_ustring(b->buf) + b->pos, b->enc, c);
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
	ustring s;
	val_check(str,string);
	l = val_strlen(str);
	s = val_ustring(str);
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
		return alloc_bool((l & 1) == 0);
	case UCS2_BE:
		if( s[0] == 0xFF && s[1] == 0xFE ) return val_false; // BOM fail
		return alloc_bool((l & 1) == 0);
	case UTF32_LE:
		if( l >= 4 && s[0] == 0 && s[1] == 0 && s[2] == 0xFE && s[3] == 0xFF ) return val_false; // BOM fail
		return alloc_bool((l & 3) == 0);	
	case UTF32_BE:
		if( l >= 4 && s[0] == 0xFF && s[1] == 0xFE && s[2] == 0 && s[3] == 0 ) return val_false; // BOM fail
		return alloc_bool((l & 3) == 0);	
	case UTF16_LE:
		if( s[0] == 0xFE && s[1] == 0xFF ) return val_false; // BOM fail
		{
		ustring end = s + l;
		int dec = IS_BE(UTF16_LE) ? 0 : 8;
		while( s + 1 < end ) {
			unsigned short c = *(unsigned short*)s;
			if( ((c >> dec) & 0xFC) == 0xD8 ) {
				if( s + 3 >= end ) return val_false;
				if( ((((unsigned short*)s)[1] >> dec) & 0xFC) != 0xDC ) return val_false;			
				s += 4;
			} else
				s += 2;
		}
		return alloc_bool(s == end);
		}
	case UTF16_BE:
		if( s[0] == 0xFF && s[1] == 0xFE ) return val_false; // BOM fail
		{
		ustring end = s + l;
		int dec = IS_BE(UTF16_BE) ? 8 : 0;
		while( s + 1 < end ) {
			unsigned short c = *(unsigned short*)s;
			if( ((c >> dec) & 0xFC) == 0xD8 ) {
				if( s + 3 >= end ) return val_false;
				if( ((((unsigned short*)s)[1] >> dec) & 0xFC) != 0xDC ) return val_false;
				s += 4;
			} else
				s += 2;
		}
		return alloc_bool(s == end);
		}
	case UTF8:
		{
		ustring end = s + l;
		while( s < end ) {
			int l = utf8_codelen[(*s) >> 4];
			if( l == 0 ) return val_false;
			s += l;
		}
		return alloc_bool(s == end);
		}
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
static value unicode_length( value str, value enc ) {
	int l;
	int count = 0;
	encoding e;
	ustring s;
	val_check(str,string);
	l = val_strlen(str);
	s = val_ustring(str);
	e = get_encoding(enc);
	switch( e ) {
	case ASCII:
	case ISO_LATIN1:
		count = l;
		break;
	case UCS2_LE:
	case UCS2_BE:
		count = l >> 1;
		break;
	case UTF16_LE:
	case UTF16_BE:
		{
		ustring end = s + l;
		int dec = IS_BE(e) ? 0 : 8;
		while( s + 1 < end ) {
			unsigned short c = *(unsigned short *)s;
			if( ((c >> dec) & 0xFC) == 0xD8 )
				s += 4;
			else
				s += 2;
			count++;
		}
		if( s > end ) count--;
		}
		break;
	case UTF32_LE:
	case UTF32_BE:
		count = l >> 2;
		break;
	case UTF8:
		{
		ustring end = s + l;
		while( s < end ) {
			int l = utf8_codelen[(*s) >> 4];
			if( l == 0 ) l = 1;
			count++;
			s += l;
		}
		if( s > end ) count--;
		}
		break;
	default:
		TODO();
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
	s = val_ustring(str);
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
	uchar c;
	ustring s;
	val_check(str,string);
	val_check(pos,int);
	s = val_ustring(str);
	c = uchar_get(&s, val_strlen(str), get_encoding(enc), val_int(pos));
	if( c == INVALID_CHAR ) neko_error();
	return alloc_best_int(c);
}

/**
	unicode_iter : string -> encoding:int -> f:(int -> void) -> void
	<doc>Call [f] with each of Unicode char of the string.</doc>
**/
static value unicode_iter( value str, value enc, value f ) {
	encoding e;
	ustring s, end;
	val_check(str,string);
	val_check_function(f,1);
	e = get_encoding(enc);
	s = val_ustring(str);
	end = s + val_strlen(str);
	while( s < end ) {
		uchar c = uchar_get(&s, end - s, e, 0);
		if( c == INVALID_CHAR ) neko_error();
		val_call1(f,alloc_best_int(c));
	}
	return val_null;
}

/**
	unicode_compare : s1:string -> s2:string -> encoding:int -> int
	<doc>Compare two Unicode strings according to their char codes.</doc>
**/
static value unicode_compare( value str1, value str2, value enc ) {
	int l1, l2, l;
	encoding e;
	ustring s1, s2;
	val_check(str1,string);
	val_check(str2,string);
	l1 = val_strlen(str1);
	l2 = val_strlen(str2);
	s1 = val_ustring(str1);
	s2 = val_ustring(str2);
	l = (l1 < l2)?l1:l2;
	e = get_encoding(enc);
	switch( e ) {
	case ISO_LATIN1:
	case ASCII:
	case UCS2_BE:
	case UTF32_BE: {
		int r = memcmp(s1,s2,l);
		if( r != 0 )
			return alloc_int(r);
		break;
		}
	case UCS2_LE: {
		unsigned short *i1 = (unsigned short*)s1;
		unsigned short *i2 = (unsigned short*)s2;
		int i, d;
		for( i = 0; i < (l>>1); i++ ) {
			unsigned short c1 = i1[i];
			unsigned short c2 = i2[i];
			if( IS_BE(UCS2_LE) ) {
				c1 = u16be(c1);
				c2 = u16be(c2);
			}
			d = c1 - c2;
			if( d != 0 )
				return alloc_int(d < 0 ? -1 : 1);
		}
		break;
		}
	case UTF32_LE: {
		unsigned int *i1 = (unsigned int*)s1;
		unsigned int *i2 = (unsigned int*)s2;
		int i, d;
		for( i = 0; i < (l>>2); i++ ) {
			unsigned int c1 = i1[i];
			unsigned int c2 = i2[i];
			if( IS_BE(UTF32_LE) ) {
				c1 = u32be(c1);
				c2 = u32be(c2);
			}
			d = c1 - c2;
			if( d != 0 )
				return alloc_int(d < 0 ? -1 : 1);
		}
		break;
		}
	case UTF16_BE:
	case UTF16_LE: {
		unsigned short *i1 = (unsigned short*)s1;
		unsigned short *i2 = (unsigned short*)s2;
		unsigned short *end1 = i1 + (l1>>1);
		unsigned short *end2 = i2 + (l2>>1);
		int d;
		while( i1 < end1 && i2 < end2 ) {
			unsigned short c1 = *i1++;
			unsigned short c2 = *i2++;
			if( IS_BE(e) ) {
				c1 = u16be(c1);
				c2 = u16be(c2);
			}
			if( (c1 & 0xFC00) == 0xD800 ) {
				if( i1 == end1 ) neko_error();
				c1 = (((c1&0x3FFF)<<10) | ((*i1++)&0x3FFF)) + 0x10000;
			}
			if( (c2 & 0xFC00) == 0xD800 ) {
				if( i2 == end2 ) neko_error();
				c2 = (((c2&0x3FFF)<<10) | ((*i2++)&0x3FFF)) + 0x10000;
			}
			d = c1 - c2;
			if( d != 0 )
				return alloc_int(d < 0 ? -1 : 1);
		}
		d = (end1 - i1) - (end2 - i2);
		return alloc_int( d == 0 ? 0 : d < 0 ? -1 : 1 );
		}
	case UTF8:
		// assume that we have correctly encoded the code points
		// so they both take the minimum required number of stored bytes
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
	default:
		TODO();
		break;
	}
	if( l1 != l2 )
		return alloc_int((l1 > l2)?1:-1);
	return alloc_int(0);
}

static void expand( value *str, int *len ) {
	int len2 = ((*len) * 5) >> 2;
	value v2;
	if( len2 - (*len) < 10 ) len2 = (*len) + 10;
	v2 = alloc_empty_string(len2);
	memcpy(val_string(v2), val_string(*str), *len);
	*len = len2;
	*str = v2;
}

/**
	unicode_convert : string -> encoding:int -> to_encoding:int -> string
	<doc>Convert an Unicode string from a given encoding to another.</doc>
**/
static value unicode_convert( value str, value encoding, value to_encoding ) {
	ustring s, end;
	int e, e_to;
	int len, size, pos = 0;
	value vto;
	val_check(str,string);
	s = val_ustring(str);
	size = val_strlen(str);
	end = s + size;
	e = get_encoding(encoding);
	e_to = get_encoding(to_encoding);
	if( e == e_to )
		return str;
	// try to allocate enough space at first guess
	switch( e ) {	
	case ISO_LATIN1:
	case UTF8:
	case ASCII:
		len = size;
		break;
	case UCS2_LE:
	case UCS2_BE:
	case UTF16_LE:
	case UTF16_BE:
		len = size >> 1;
		break;
	case UTF32_LE:
	case UTF32_BE:
		len = size >> 2;
		break;
	default:
		TODO();
		len = 0;
		break;
	}
	switch( e_to ) {
	case ISO_LATIN1:
	case ASCII:
		break;
	case UTF8:
		// assume one byte per char
		break;
	case UCS2_LE:
	case UCS2_BE:
	case UTF16_LE:
	case UTF16_BE:
		len *= 2;
		break;
	case UTF32_LE:
	case UTF32_BE:
		len *= 4;
		break;
	default:
		TODO();
		break;
	}
	// convert
	vto = alloc_empty_string(len);
	while( s < end ) {
		uchar c = uchar_get(&s,end - s,e,0);
		int k;
		if( c == INVALID_CHAR ) val_throw(alloc_string("Input string is not correctly encoded"));
		k = uchar_size(c, e_to);
		if( k == 0 ) {
			if( e_to == ISO_LATIN1 || e_to == ASCII )
				c = '?';
			else
				c = 0xFFFD;
			k = uchar_size(c, e_to);
		}
		if( pos + k > len )
			expand(&vto,&len);
		uchar_set(val_string(vto) + pos, e_to, c);
		pos += k;
	}
	val_set_length(vto, pos);
	val_string(vto)[pos] = 0;
	return vto;
}

/**
	unicode_lower : string -> encoding:int -> string
	<doc>Returns the lowercase version of the unicode string.</doc>
**/
static value unicode_lower( value str, value enc ) {
	ustring s,end;
	value out;
	int pos = 0;
	int len;
	encoding e;
	val_check(str,string);
	e = get_encoding(enc);
	s = val_ustring(str);
	len = val_strlen(str);
	end = s + len;
	out = alloc_empty_string(len);
	while( s < end ) {
		uchar c = uchar_get(&s,end - s, e, 0);
		int up = c >> UL_BITS;
		int k;
		if( up < LMAX ) {
			uchar c2 = LOWER[up][c&((1<<UL_BITS)-1)];
			if( c2 != 0 ) c = c2;
		}
		k = uchar_size(c, e);
		if( pos + k > len )
			expand(&out, &len);
		uchar_set(val_ustring(out)+pos, e, c);
		pos += k;
	}
	val_set_length(out, pos);
	val_string(out)[pos] = 0;
	return out;
}

/**
	unicode_upper : string -> encoding:int -> string
	<doc>Returns the lowercase version of the unicode string.</doc>
**/
static value unicode_upper( value str, value enc ) {
	ustring s,end;
	value out;
	int pos = 0;
	int len;
	encoding e;
	val_check(str,string);
	e = get_encoding(enc);
	s = val_ustring(str);
	len = val_strlen(str);
	end = s + len;
	out = alloc_empty_string(len);
	while( s < end ) {
		uchar c = uchar_get(&s,end - s, e, 0);
		int up = c >> UL_BITS;
		int k;
		if( up < UMAX ) {
			uchar c2 = UPPER[up][c&((1<<UL_BITS)-1)];
			if( c2 != 0 ) c = c2;
		}
		k = uchar_size(c, e);
		if( pos + k > len )
			expand(&out, &len);
		uchar_set(val_ustring(out)+pos, e, c);
		pos += k;
	}
	val_set_length(out, pos);
	val_string(out)[pos] = 0;
	return out;
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
DEFINE_PRIM(unicode_lower,2);
DEFINE_PRIM(unicode_upper,2);

/* ************************************************************************ */
