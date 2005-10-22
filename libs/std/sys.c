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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#ifdef _WIN32
#	include <direct.h>
#else
#	include <unistd.h>
#endif

static value get_env( value v ) {
	char *s;
	val_check(v,string);
	s = getenv(val_string(v));
	if( s == NULL )
		return val_null;
	return alloc_string(s);
}

static value set_locale( value l ) {
	val_check(l,string);
	return alloc_bool(setlocale(LC_TIME,val_string(l)) != NULL);
}

static value get_cwd() {
	char buf[256];
	int l;
	if( getcwd(buf,256) == NULL )
		neko_error();
	l = (int)strlen(buf);
	if( buf[l-1] != '/' && buf[l-1] != '\\' ) {
		buf[l] = '/';
		buf[l+1] = 0;
	}
	return alloc_string(buf);
}

static value set_cwd( value d ) {
	val_check(d,string);
	if( chdir(val_string(d)) )
		neko_error();
	return val_true;
}

static value sys_command( value cmd ) {
	val_check(cmd,string);
	if( val_strlen(cmd) == 0 )
		return alloc_int(-1);
	return alloc_int( system(val_string(cmd)) );
}

static value sys_exit( value ecode ) {
	val_check(ecode,int);
	exit(val_int(ecode));
	return val_true;
}

DEFINE_PRIM(get_env,1);
DEFINE_PRIM(set_locale,1);
DEFINE_PRIM(get_cwd,0);
DEFINE_PRIM(set_cwd,1);
DEFINE_PRIM(sys_command,1);
DEFINE_PRIM(sys_exit,1);

/* ************************************************************************ */
