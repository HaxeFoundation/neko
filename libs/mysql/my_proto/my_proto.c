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

int my_recv( MYSQL *m, void *buf, int size ) {
	while( size ) {
		int len = psock_recv(m->s,(char*)buf,size);
		if( len < 0 ) return 0;
		buf = ((char*)buf) + len;
		size -= len;
	}
	return 1;
}


int my_send( MYSQL *m, void *buf, int size ) {
	while( size ) {
		int len = psock_send(m->s,(char*)buf,size);
		if( len < 0 ) return 0;
		buf = ((char*)buf) + len;
		size -= len;
	}
	return 1;
}

int my_read( MYSQL_PACKET *p, void *buf, int size ) {
	if( p->size - p->pos < size ) {
		p->error = 1;
		return 0;
	}
	memcpy(buf,p->buf + p->pos,size);
	p->pos += size;
	return 1;
}

const char *my_read_string( MYSQL_PACKET *p ) {
	char *str;
	if( p->pos >= p->size ) {
		p->error = 1;
		return "";
	}
	str = p->buf + p->pos;
	p->pos += strlen(str) + 1;
	return str;
}

int my_read_packet( MYSQL *m, MYSQL_PACKET *p ) {
	unsigned int psize;
	p->pos = 0;
	p->error = 0;
	if( !my_recv(m,&psize,4) ) {
		p->id = -1;
		p->error = 1;
		p->size = 0;
		return 0;
	}
	p->id = (psize >> 24);
	psize &= 0xFFFFFF;
	p->size = psize;
	if( p->mem < (int)psize ) {
		free(p->buf);
		p->buf = (char*)malloc(psize + 1);
		p->mem = psize;
	}
	p->buf[psize] = 0;
	if( !my_recv(m,p->buf,psize) ) {
		p->error = 1;
		p->size = 0;
		p->buf[0] = 0;
		return 0;
	}
	return 1;
}

int my_send_packet( MYSQL *m, MYSQL_PACKET *p ) {
	unsigned int header = p->size | (p->id << 24);
	if( !my_send(m,&header,4) ) {
		p->error = 1;
		return 0;
	}
	if( !my_send(m,p->buf,p->size) ) {
		p->error = 1;
		return 0;
	}
	return 1;
}

void my_begin_packet( MYSQL_PACKET *p, int id, int minsize ) {
	if( p->mem < minsize ) {
		free(p->buf);
		p->buf = (char*)malloc(minsize + 1);
		p->mem = minsize;
	}
	p->error = 0;
	p->id = id;
	p->size = 0;
}

void my_write( MYSQL_PACKET *p, const void *data, int size ) {
	if( p->size + size > p->mem ) {
		char *buf2;
		if( p->mem == 0 ) p->mem = 32;
		do {
			p->mem <<= 1;
		} while( p->size + size <= p->mem );
		buf2 = (char*)malloc(p->mem);
		memcpy(buf2,p->buf,p->size);
		free(p->buf);
		p->buf = buf2;
	}
	memcpy( p->buf + p->size , data, size );
	p->size += size;
}

void my_write_string( MYSQL_PACKET *p, const char *str ) {
	my_write(p,str,strlen(str) + 1);
}

void my_write_bin( MYSQL_PACKET *p, const void *data, int size ) {
	if( size <= 250 ) {
		unsigned char l = (unsigned char)size;
		my_write(p,&l,1);
	} else if( size < 0x10000 ) {
		unsigned char c = 252;
		unsigned short l = (unsigned short)size;
		my_write(p,&c,1);
		my_write(p,&l,2);
	} else if( size < 0x1000000 ) {
		unsigned char c = 253;
		unsigned int l = (unsigned short)size;
		my_write(p,&c,1);
		my_write(p,&l,3);
	} else {
		unsigned char c = 254;
		my_write(p,&c,1);
		my_write(p,&size,4);
	}
	my_write(p,data,size);
}

void my_crypt( unsigned char *out, const unsigned char *s1, const unsigned char *s2, unsigned int len ) {
	unsigned int i;
	for(i=0;i<len;i++)
		out[i] = s1[i] ^ s2[i];
}

void my_encrypt_password( const char *pass, const char *seed, SHA1_DIGEST out ) {
	SHA1_CTX ctx;
	SHA1_DIGEST hash_stage1, hash_stage2;
	// stage 1: hash password
	sha1_init(&ctx);
	sha1_update(&ctx,pass,strlen(pass));;
	sha1_final(&ctx,hash_stage1);
	// stage 2: hash stage 1; note that hash_stage2 is stored in the database
	sha1_init(&ctx);
	sha1_update(&ctx, hash_stage1, SHA1_SIZE);
	sha1_final(&ctx, hash_stage2);
	// create crypt string as sha1(message, hash_stage2)
	sha1_init(&ctx);
	sha1_update(&ctx, seed, SHA1_SIZE);
	sha1_update(&ctx, hash_stage2, SHA1_SIZE);
	sha1_final( &ctx, out );
	// xor the result
	my_crypt(out,out,hash_stage1,SHA1_SIZE);
}

/* ************************************************************************ */
