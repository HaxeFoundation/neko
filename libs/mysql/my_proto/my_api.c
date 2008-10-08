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
#include "my_proto.h"
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

static void error( MYSQL *m, const char *err, const char *param ) {
	if( param ) {
		unsigned int max = MAX_ERR_SIZE - 3 - strlen(err);
		if( strlen(param) > max ) {
			char *p2 = (char*)malloc(max + 1);
			memcpy(p2,param,max-3);
			p2[max - 3] = '.';
			p2[max - 2] = '.';
			p2[max - 1] = '.';
			p2[max] = 0;
			sprintf(m->last_error,err,param);
			free(p2);
			return;
		}
	}
	sprintf(m->last_error,err,param);
	m->errcode = -1;
}

static int my_ok( MYSQL *m ) {
	unsigned char code;
	MYSQL_PACKET *p = &m->packet;
	if( !my_read_packet(m,p) || m->packet.size == 0 ) {
		error(m,"Failed to read packet",NULL);
		return 0;
	}
	my_read(p,&code,1);
	if( code == 0x00 )
		return 1;
	if( code == 0xFF ) {
		unsigned short ecode = -1;
		my_read(p,&ecode,2);
		if( m->is41 && p->buf[p->pos] == '#' )
			p->pos += 6; // skip sqlstate marker
		error(m,"%s",my_read_string(p));
		m->errcode = ecode;
	} else
		error(m,"Invalid packet error",NULL);
	return 0;
}

static void my_close( MYSQL *m ) {
	psock_close(m->s);
	m->s = INVALID_SOCKET;
}

MYSQL *mysql_init( void *unused ) {
	MYSQL *m = (MYSQL*)malloc(sizeof(struct _MYSQL));
	psock_init();
	memset(m,0,sizeof(struct _MYSQL));
	m->s = INVALID_SOCKET;
	error(m,"NO ERROR",NULL);
	m->errcode = 0;
	return m;
}

MYSQL *mysql_real_connect( MYSQL *m, const char *host, const char *user, const char *pass, void *unused, int port, const char *socket, int options ) {
	PHOST h = phost_resolve(host);
	MYSQL_PACKET *p = &m->packet;
	if( h == UNRESOLVED_HOST ) {
		error(m,"Failed to resolve host '%s'",host);
		return NULL;
	}
	m->s = psock_create();
	if( m->s == INVALID_SOCKET ) {
		error(m,"Failed to create socket",NULL);
		return NULL;
	}
	if( psock_connect(m->s,h,port) != PS_OK ) {
		my_close(m);
		error(m,"Failed to connect on host '%s'",host);
		return NULL;
	}
	if( !my_read_packet(m,p) ) {
		my_close(m);
		error(m,"Failed to read handshake packet",NULL);
		return NULL;
	}
	// process handshake packet
	{
		char scramble_buf[21];
		char fill_char;
		char filler[13];
		my_read(p,&m->infos.proto_version,1);
		m->infos.server_version = strdup(my_read_string(p));
		my_read(p,&m->infos.thread_id,4);
		my_read(p,scramble_buf,8);
		my_read(p,&fill_char,1);
		if( fill_char != 0 ) p->error = 1;
		my_read(p,&m->infos.server_flags,2);
		my_read(p,&m->infos.server_charset,1);
		my_read(p,&m->infos.server_status,2);
		my_read(p,filler,13);
		m->is41 = (m->infos.server_flags & FL_PROTOCOL_41) != 0;
		if( !p->error && m->is41 )
			my_read(p,scramble_buf + 8,13);
		if( p->error || p->pos != p->size ) {
			my_close(m);
			error(m,"Failed to decode server handshake",NULL);
			return NULL;
		}
		// fill answer packet
		{
			unsigned int flags = m->infos.server_flags;
			int max_packet_size = 0x01000000;
			SHA1_DIGEST hpass;
			char filler[23];
			flags &= (FL_PROTOCOL_41 | FL_TRANSACTIONS | FL_SECURE_CONNECTION);
			my_begin_packet(p,p->id + 1,128);
			my_write(p,&flags,4);
			my_write(p,&max_packet_size,4);
			my_write(p,&m->infos.server_charset,1);
			memset(filler,0,23);
			my_write(p,filler,23);
			my_write_string(p,user);
			if( *pass ) {
				my_encrypt_password(pass,scramble_buf,hpass);
				my_write_bin(p,hpass,SHA1_SIZE);
			} else
				my_write_bin(p,NULL,0);
		}
	}
	// send connection packet
	my_send_packet(m,p);
	if( p->error ) {
		my_close(m);
		error(m,"Failed to send connection packet",NULL);
		return NULL;
	}
	// read answer packet
	if( !my_ok(m) ) {
		my_close(m);
		return NULL;
	}
	return m;
}

int mysql_select_db( MYSQL *m, const char *dbname ) {
	return -1;
}

int mysql_real_query( MYSQL *m, const char *query, int qlength ) {
	return -1;
}

MYSQL_RES *mysql_store_result( MYSQL *m ) {
	return NULL;
}

int mysql_field_count( MYSQL *m ) {
	return -1;
}

int mysql_affected_rows( MYSQL *m ) {
	return -1;
}

int mysql_real_escape_string( MYSQL *m, char *sout, const char *sin, int length ) {
	memcpy(sout,sin,length);
	return length;
}

void mysql_close( MYSQL *m ) {
	my_close(m);
	free(m->packet.buf);
	free(m->infos.server_version);
	free(m);
}

const char *mysql_error( MYSQL *m ) {
	return m->last_error;
}

// RESULTS API

unsigned int mysql_num_rows( MYSQL_RES *r ) {
	return 0;
}

int mysql_num_fields( MYSQL_RES *r ) {
	return 0;
}

MYSQL_FIELD *mysql_fetch_fields( MYSQL_RES *r ) {
	return NULL;
}

unsigned long *mysql_fetch_lengths( MYSQL_RES *r ) {
	return NULL;
}

MYSQL_ROW mysql_fetch_row( MYSQL_RES * r ) {
	return NULL;
}

void mysql_free_result( MYSQL_RES *r ) {
}

/* ************************************************************************ */
