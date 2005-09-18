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
#include <string.h>

#ifndef _WIN32
#  include <strings.h>
#  undef strcmpi
#  define strcmpi(a,b) strcasecmp(a,b)
#endif

DEFINE_KIND(k_xml);

// -------------- parsing --------------------------

typedef enum {
	IGNORE_SPACES,
	BEGIN,
	BEGIN_NODE,
	TAG_NAME,
	BODY,
	ATTRIB_NAME,
	EQUALS,
	ATTVAL_BEGIN,
	ATTRIB_VAL,
	ESCAPE,
	CHILDS,
	CLOSE,
	WAIT_END,
	PCDATA,
	COMMENT
} STATE;

typedef struct {
	value fxml;
	value fpcdata;
	value fcomment;
	value fdone;
} functions;

static bool is_valid_char( int c ) {
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == ':' || c == '.' || c == '_' || c == '-';
}

static value do_parse_xml( const char **lp, functions *f, value parent, const char *parentname ) {
	STATE state = IGNORE_SPACES;
	STATE next = BEGIN;	
	field aname;
	value cur;
	value attribs;
	value nodename;
	const char *start;
	const char *p = *lp;
	char c = *p;
	while( true ) {
		switch( state ) {
		case IGNORE_SPACES:
			switch( c ) {
			case '\n':
			case '\r':
			case '\t':
			case ' ':
				break;
			default:
				state = next;
				continue;
			}
			break;
		case BEGIN:
			switch( c ) {
			case '<':
				state = IGNORE_SPACES;
				next = BEGIN_NODE;
				break;
			default:
				start = p;
				state = PCDATA;
				continue;
			}
			break;
		case PCDATA:
			if( c == '<' ) {
				if( val_is_null(parent) )
					return NULL;
				val_call2(f->fpcdata,parent,copy_string(start,p-start));
				state = IGNORE_SPACES;
				next = BEGIN_NODE;
			}
			break;
		case BEGIN_NODE:
			switch( c ) {
			case '!':
				state = COMMENT;
				start = p + 1;
				break;
			case '/':
				if( parent == NULL )
					return NULL;
				start = p + 1;
				state = IGNORE_SPACES;
				next = CLOSE;
				break;
			default:
				state = TAG_NAME;
				start = p;
				continue;
			}
			break;
		case TAG_NAME:
			if( !is_valid_char(c) ) {
				if( p == start )
					return NULL;
				nodename = copy_string(start,p-start);
				attribs = alloc_object(NULL);
				state = IGNORE_SPACES;
				next = BODY;
				continue;
			}
			break;
		case BODY:
			switch( c ) {
			case '/':
				state = WAIT_END;
				cur = val_call3(f->fxml,parent,nodename,attribs);
				break;
			case '>':
				state = IGNORE_SPACES;
				next = CHILDS;
				cur = val_call3(f->fxml,parent,nodename,attribs);
				break;
			default:
				state = ATTRIB_NAME;
				start = p;
				continue;
			}
			break;
		case ATTRIB_NAME:
			if( !is_valid_char(c) ) {
				value tmp;
				if( start == p )
					return NULL;
				tmp = copy_string(start,p-start);
				aname = val_id(val_string(tmp));
				state = IGNORE_SPACES;
				next = EQUALS;
				continue;
			}
			break;
		case EQUALS:
			switch( c ) {
			case '=':
				state = IGNORE_SPACES;
				next = ATTVAL_BEGIN;
				break;
			default:
				return NULL;
			}
			break;
		case ATTVAL_BEGIN:
			switch( c ) {
			case '"':
			case '\'':
				state = ATTRIB_VAL;
				start = p;
				break;
			default:
				return NULL;
			}
			break;
		case ATTRIB_VAL:
			if( c == *start ) {
				value aval = copy_string(start+1,p-start-1);
				alloc_field(attribs,aname,aval);
				state = IGNORE_SPACES;
				next = BODY;
			}
			break;
		case CHILDS:
			*lp = p;
			while( true ) {
				value x = do_parse_xml(lp,f,cur,val_string(nodename));
				if( x == NULL )
					return NULL;
				if( x == cur ) {
					val_call1(f->fdone,cur);
					return cur;
				}
			}
			break;
		case WAIT_END:
			switch( c ) {
			case '>': 
				*lp = p+1;
				val_call1(f->fdone,cur);
				return cur;
			default :
				return NULL;
			}
			break;
		case CLOSE:
			if( !is_valid_char(c) ) {
				if( c != '>' )
					return NULL;
				if( start == p )
					return NULL;
				{
					value v = copy_string(start,p - start);
					if( strcmpi(parentname,val_string(v)) != 0 )
						return NULL;
				}
				*lp = p+1;
				return parent;
			}
			break;
		case COMMENT:
			if( c == '>' ) {
				val_call2(f->fcomment,parent,copy_string(start,p-start));
				state = IGNORE_SPACES;
				next = BEGIN;
			}
			break;
		}
		c = *++p;
		if( c == 0 )
			return NULL;
	}
}

// ----------------------------------------------

static value parse_xml( value str, value fxml, value fpcdata, value fcomment, value fdone ) {
	const char *p;
	functions f;
	value v;
	val_check(str,string);
	val_check_function(fxml,3);
	val_check_function(fpcdata,2);
	val_check_function(fcomment,2);
	val_check_function(fdone,1);
	f.fxml = fxml;
	f.fpcdata = fpcdata;
	f.fcomment = fcomment;
	f.fdone = fdone;
	p = val_string(str);
	// skip BOM
	if( p[0] == (char)0xEF && p[1] == (char)0xBB && p[2] == (char)0xBF )
		p += 3;
	v = do_parse_xml(&p,&f,val_null,NULL);
	if( v == NULL )
		type_error();
	return v;
}

DEFINE_PRIM(parse_xml,3);

/* ************************************************************************ */
