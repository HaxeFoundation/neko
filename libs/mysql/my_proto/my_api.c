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
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include "my_proto.h"

static void error( MYSQL *m, const char *err, const char *param ) {
	if( param ) {
		unsigned int max = MAX_ERR_SIZE - (strlen(err) + 3);
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

static void save_error( MYSQL *m, MYSQL_PACKET *p ) {
	int ecode;
	p->pos = 1;
	ecode = my_read_ui16(p);
	if( m->is41 && p->buf[p->pos] == '#' )
		p->pos += 6; // skip sqlstate marker
	error(m,"%s",my_read_string(p));
	m->errcode = ecode;
}

static int my_ok( MYSQL *m, int allow_others ) {
	int code;
	MYSQL_PACKET *p = &m->packet;
	if( !my_read_packet(m,p) || m->packet.size == 0 ) {
		error(m,"Failed to read packet",NULL);
		return 0;
	}
	code = my_read_byte(p);
	if( code == 0x00 )
		return 1;
	if( code == 0xFF )
		save_error(m,p);
	else if( allow_others )
		return 1;
	else
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
	m->last_field_count = -1;
	m->last_insert_id = -1;
	m->affected_rows = -1;
	return m;
}

MYSQL *mysql_real_connect( MYSQL *m, const char *host, const char *user, const char *pass, void *unused, int port, const char *socket, int options ) {
	PHOST h;
	char scramble_buf[21];
	MYSQL_PACKET *p = &m->packet;
	if( socket ) {
		error(m,"Unix Socket connections are not supported",NULL);
		return NULL;
	}
	h = phost_resolve(host);
	if( h == UNRESOLVED_HOST ) {
		error(m,"Failed to resolve host '%s'",host);
		return NULL;
	}
	m->s = psock_create();
	if( m->s == INVALID_SOCKET ) {
		error(m,"Failed to create socket",NULL);
		return NULL;
	}
	psock_set_fastsend(m->s,1);
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
		char filler[13];
		m->infos.proto_version = my_read_byte(p);
		m->infos.server_version = strdup(my_read_string(p));
		m->infos.thread_id = my_read_int(p);
		my_read(p,scramble_buf,8);
		if( my_read_byte(p) != 0 ) p->error = 1;
		m->infos.server_flags = my_read_ui16(p);
		m->infos.server_charset = my_read_byte(p);
		m->infos.server_status = my_read_ui16(p);
		my_read(p,filler,13);
		// try to disable 41
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
			my_begin_packet(p,1,128);
			if( m->is41 ) {
				my_write_int(p,flags);
				my_write_int(p,max_packet_size);
				my_write_byte(p,m->infos.server_charset);
				memset(filler,0,23);
				my_write(p,filler,23);
				my_write_string(p,user);
				if( *pass ) {
					my_encrypt_password(pass,scramble_buf,hpass);
					my_write_bin(p,SHA1_SIZE);
					my_write(p,hpass,SHA1_SIZE);
					my_write_byte(p,0);
				} else
					my_write_bin(p,0);
			} else {
				my_write_ui16(p,flags);
				// max_packet_size
				my_write_byte(p,0xFF);
				my_write_byte(p,0xFF);
				my_write_byte(p,0xFF);
				my_write_string(p,user);
				if( *pass ) {
					char hpass[SEED_LENGTH_323 + 1];
					my_encrypt_pass_323(pass,scramble_buf,hpass);
					hpass[SEED_LENGTH_323] = 0;
					my_write(p,hpass,SEED_LENGTH_323 + 1);
				} else
					my_write_bin(p,0);
			}
		}
	}
	// send connection packet
send_cnx_packet:
	if( !my_send_packet(m,p) ) {
		my_close(m);
		error(m,"Failed to send connection packet",NULL);
		return NULL;
	}
	// read answer packet
	if( !my_read_packet(m,p) || m->packet.size == 0 ) {
		my_close(m);
		error(m,"Failed to read packet",NULL);
		return NULL;
	}
	// process answer
	{
		int code = my_read_byte(p);
		switch( code ) {
		case 0: // OK packet
			break;
		case 0xFF: // ERROR
			my_close(m);
			save_error(m,p);
			return NULL;
		case 0xFE: // EOF
			// we are asked to send old password authentification
			if( p->size == 1 ) {
				char hpass[SEED_LENGTH_323 + 1];
				my_encrypt_pass_323(pass,scramble_buf,hpass);
				hpass[SEED_LENGTH_323] = 0;
				my_begin_packet(p,3,0);
				my_write(p,hpass,SEED_LENGTH_323 + 1);
				goto send_cnx_packet;
			}
			// fallthrough
		default:
			my_close(m);
			error(m,"Invalid packet error",NULL);
			return NULL;
		}
	}
	return m;
}

int mysql_select_db( MYSQL *m, const char *dbname ) {
	MYSQL_PACKET *p = &m->packet;
	my_begin_packet(p,0,0);
	my_write_byte(p,COM_INIT_DB);
	my_write_string(p,dbname);
	if( !my_send_packet(m,p) ) {
		error(m,"Failed to send packet",NULL);
		return -1;
	}
	return my_ok(m,0) ? 0 : -1;
}

int mysql_real_query( MYSQL *m, const char *query, int qlength ) {
	MYSQL_PACKET *p = &m->packet;
	my_begin_packet(p,0,0);
	my_write_byte(p,COM_QUERY);
	my_write(p,query,qlength);
	m->last_field_count = -1;
	m->affected_rows = -1;
	m->last_insert_id = -1;
	if( !my_send_packet(m,p) ) {
		error(m,"Failed to send packet",NULL);
		return -1;
	}
	if( !my_ok(m,1) )
		return -1;
	p->id = IS_QUERY;
	return 0;
}

static int do_store( MYSQL *m, MYSQL_RES *r ) {
	int i;
	MYSQL_PACKET *p = &m->packet;
	p->pos = 0;
	r->nfields = my_read_bin(p);
	if( p->error ) return 0;
	r->fields = (MYSQL_FIELD*)malloc(sizeof(MYSQL_FIELD) * r->nfields);
	memset(r->fields,0,sizeof(MYSQL_FIELD) * r->nfields);
	for(i=0;i<r->nfields;i++) {
		if( !my_read_packet(m,p) )
			return 0;
		{
			MYSQL_FIELD *f = r->fields + i;
			f->catalog = m->is41 ? my_read_bin_str(p) : NULL;
			f->db = m->is41 ? my_read_bin_str(p) : NULL;
			f->table = my_read_bin_str(p);
			f->org_table = m->is41 ? my_read_bin_str(p) : NULL;
			f->name = my_read_bin_str(p);
			f->org_name = m->is41 ? my_read_bin_str(p) : NULL;
			if( m->is41 ) my_read_byte(p);
			f->charset = m->is41 ? my_read_ui16(p) : 0x08;
			f->length = m->is41 ? my_read_int(p) : my_read_bin(p);
			f->type = m->is41 ? my_read_byte(p) : my_read_bin(p);
			f->flags = m->is41 ? my_read_ui16(p) : my_read_bin(p);
			f->decimals = my_read_byte(p);
			if( m->is41 ) p->error |= my_read_byte(p) != 0;
			if( m->is41 ) p->error |= my_read_byte(p) != 0;
			if( p->error )
				return 0;
		}
	}
	// first EOF packet
	if( !my_read_packet(m,p) )
		return 0;
	if( my_read_byte(p) != 0xFE || p->size >= 9 )
		return 0;
	// reset packet buffer (to prevent to store large buffer in row data)
	free(p->buf);
	p->buf = NULL;
	p->mem = 0;
	// datas
	while( 1 ) {
		if( !my_read_packet(m,p) )
			return 0;
		// EOF : end of datas
		if( p->size > 0 && (unsigned char)p->buf[0] == 0xFE && p->size < 9 )
			break;
		// allocate one more row
		if( r->row_count == r->memory_rows ) {
			MYSQL_ROW_DATA *rows;
			r->memory_rows = r->memory_rows ? (r->memory_rows << 1) : 1;
			rows = (MYSQL_ROW_DATA*)malloc(r->memory_rows * sizeof(MYSQL_ROW_DATA));
			memcpy(rows,r->rows,r->row_count * sizeof(MYSQL_ROW_DATA));
			free(r->rows);
			r->rows = rows;
		}
		// read row fields
		{
			MYSQL_ROW_DATA *current = r->rows + r->row_count++;
			int prev = 0;			
			current->raw = p->buf;
			current->lengths = (unsigned long*)malloc(sizeof(unsigned long) * r->nfields);
			current->datas = (char**)malloc(sizeof(char*) * r->nfields);
			for(i=0;i<r->nfields;i++) {
				int l = my_read_bin(p);
				if( !p->error )
					p->buf[prev] = 0;
				if( l == -1 ) {
					current->lengths[i] = 0;
					current->datas[i] = NULL;
				} else {
					current->lengths[i] = l;
					current->datas[i] = p->buf + p->pos;
					p->pos += l;
				}
				prev = p->pos;
			}
			if( !p->error )
				p->buf[prev] = 0;
		}
		// the packet buffer as been stored, don't reuse it
		p->buf = NULL;
		p->mem = 0;
		if( p->error )
			return 0;
	}
	return 1;
}

MYSQL_RES *mysql_store_result( MYSQL *m ) {
	MYSQL_RES *r;
	MYSQL_PACKET *p = &m->packet;
	if( p->id != IS_QUERY )
		return NULL;
	// OK without result
	if( p->buf[0] == 0 ) {
		p->pos = 0;
		m->last_field_count = my_read_byte(p); // 0
		m->affected_rows = my_read_bin(p);
		m->last_insert_id = my_read_bin(p);
		return NULL;
	}
	r = (MYSQL_RES*)malloc(sizeof(struct _MYSQL_RES));
	memset(r,0,sizeof(struct _MYSQL_RES));
	if( !do_store(m,r) ) {
		mysql_free_result(r);
		error(m,"Failure while storing result",NULL);
		return NULL;
	}
	m->last_field_count = r->nfields;
	return r;
}

int mysql_field_count( MYSQL *m ) {
	return m->last_field_count;
}

int mysql_affected_rows( MYSQL *m ) {
	return m->affected_rows;
}

int mysql_escape_string( MYSQL *m, char *sout, const char *sin, int length ) {
	return my_escape_string(m->infos.server_charset,sout,sin,length);
}

int mysql_real_escape_string( MYSQL *m, char *sout, const char *sin, int length ) {
	if( m->infos.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES )
		return my_escape_quotes(m->infos.server_charset,sout,sin,length);
	return my_escape_string(m->infos.server_charset,sout,sin,length);
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
	return r->row_count;
}

int mysql_num_fields( MYSQL_RES *r ) {
	return r->nfields;
}

MYSQL_FIELD *mysql_fetch_fields( MYSQL_RES *r ) {
	return r->fields;
}

unsigned long *mysql_fetch_lengths( MYSQL_RES *r ) {
	return r->current ? r->current->lengths : NULL;
}

MYSQL_ROW mysql_fetch_row( MYSQL_RES * r ) {
	if( r->current == NULL )
		r->current = r->rows;
	else
		r->current++;
	if( r->current >= r->rows + r->row_count )
		r->current = NULL;
	return r->current ? r->current->datas : NULL;
}

void mysql_free_result( MYSQL_RES *r ) {
	if( r->fields ) {
		int i;
		for(i=0;i<r->nfields;i++) {
			MYSQL_FIELD *f = r->fields + i;
			free(f->catalog);
			free(f->db);
			free(f->table);
			free(f->org_table);
			free(f->name);
			free(f->org_name);
		}
		free(r->fields);
	}
	if( r->rows ) {
		int i;
		for(i=0;i<r->row_count;i++) {
			MYSQL_ROW_DATA *row = r->rows + i;
			free(row->datas);
			free(row->lengths);
			free(row->raw);
		}
		free(r->rows);
	}
	free(r);
}

/* ************************************************************************ */
