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
#ifndef MY_PROTO_H
#define MY_PROTO_H

#include "mysql.h"
#include "socket.h"
#include "sha1.h"

typedef enum {
	FL_LONG_PASSWORD = 1,
	FL_FOUND_ROWS = 2,
	FL_LONG_FLAG = 4,
	FL_CONNECT_WITH_DB = 8,
	FL_NO_SCHEMA = 16,
	FL_COMPRESS = 32,
	FL_ODBC = 64,
	FL_LOCAL_FILES = 128,
	FL_IGNORE_SPACE	= 256,
	FL_PROTOCOL_41 = 512,
	FL_INTERACTIVE = 1024,
	FL_SSL = 2048,
	FL_IGNORE_SIGPIPE = 4096,
	FL_TRANSACTIONS = 8192,
	FL_RESERVED = 16384,
	FL_SECURE_CONNECTION = 32768,
	FL_MULTI_STATEMENTS  = 65536,
	FL_MULTI_RESULTS = 131072,
} MYSQL_FLAGS;

typedef struct {
	unsigned char proto_version;
	char *server_version;
	unsigned int thread_id;
	unsigned short server_flags;
	unsigned char server_charset;
	unsigned short server_status;
} MYSQL_INFOS;

typedef struct {
	int id;
	int error;
	int size;
	int pos;
	int mem;
	char *buf;
} MYSQL_PACKET;

#define MAX_ERR_SIZE	1024

struct _MYSQL {
	PSOCK s;
	MYSQL_INFOS infos;
	MYSQL_PACKET packet;
	int is41;
	int errcode;
	char last_error[MAX_ERR_SIZE];
};

struct _MYSQL_RES {
	int _;
};


// network
int my_recv( MYSQL *m, void *buf, int size );
int my_send( MYSQL *m, void *buf, int size );
int my_read_packet( MYSQL *m, MYSQL_PACKET *p );
int my_send_packet( MYSQL *m, MYSQL_PACKET *p );

// packet read
int my_read( MYSQL_PACKET *p, void *buf, int size );
const char *my_read_string( MYSQL_PACKET *p );

// packet write
void my_begin_packet( MYSQL_PACKET *p, int id, int minsize );
void my_write( MYSQL_PACKET *p, const void *data, int size );
void my_write_string( MYSQL_PACKET *p, const char *str );
void my_write_bin( MYSQL_PACKET *p, const void *data, int size );

// passwords
void my_crypt( unsigned char *out, const unsigned char *s1, const unsigned char *s2, unsigned int len );
void my_encrypt_password( const char *pass, const char *seed, SHA1_DIGEST out );

#endif
/* ************************************************************************ */
