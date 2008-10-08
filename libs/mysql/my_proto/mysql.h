/* ************************************************************************ */
/*																			*/
/*  MYSQL 5.0 Protocol Implementation 										*/
/*  Copyright (c)2008 Nicolas Cannasse										*/
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
#ifndef MYSQL_H
#define MYSQL_H

struct _MYSQL;
struct _MYSQL_RES;
typedef struct _MYSQL MYSQL; 
typedef struct _MYSQL_RES MYSQL_RES;
typedef char **MYSQL_ROW;

typedef enum {
	FIELD_TYPE_TINY,
	FIELD_TYPE_SHORT,
	FIELD_TYPE_LONG,
	FIELD_TYPE_INT24,
	FIELD_TYPE_LONGLONG,
	FIELD_TYPE_DECIMAL,
	FIELD_TYPE_FLOAT,
	FIELD_TYPE_DOUBLE,
	FIELD_TYPE_BLOB,
	FIELD_TYPE_TINY_BLOB,
	FIELD_TYPE_MEDIUM_BLOB,
	FIELD_TYPE_LONG_BLOB,
	FIELD_TYPE_DATE,
	FIELD_TYPE_DATETIME,
	FIELD_TYPE_NULL,
	FIELD_TYPE_ENUM,
	FIELD_TYPE_SET,
} FIELD_TYPE;

typedef struct {
	char *name;
	FIELD_TYPE type;
	int length;
} MYSQL_FIELD;

MYSQL *mysql_init( void * );
MYSQL *mysql_real_connect( MYSQL *m, const char *host, const char *user, const char *pass, void *unused, int port, const char *socket, int options );
int mysql_select_db( MYSQL *m, const char *dbname );
int mysql_real_query( MYSQL *m, const char *query, int qlength );
MYSQL_RES *mysql_store_result( MYSQL *m );
int mysql_field_count( MYSQL *m );
int mysql_affected_rows( MYSQL *m );
int mysql_real_escape_string( MYSQL *m, char *sout, const char *sin, int length );
void mysql_close( MYSQL *m );
const char *mysql_error( MYSQL *m );

unsigned int mysql_num_rows( MYSQL_RES *r );
int mysql_num_fields( MYSQL_RES *r );
MYSQL_FIELD *mysql_fetch_fields( MYSQL_RES *r );
unsigned long *mysql_fetch_lengths( MYSQL_RES *r );
MYSQL_ROW mysql_fetch_row( MYSQL_RES * r );
void mysql_free_result( MYSQL_RES *r );

#endif
/* ************************************************************************ */
