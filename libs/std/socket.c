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
#include <string.h>
#include <neko.h>
#ifdef _WIN32
#	include <winsock2.h>
	static bool init_done = false;
	static WSADATA init_data;
#else
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
	typedef int SOCKET;
#	define closesocket close
#	define SOCKET_ERROR (-1)
#	define INVALID_SOCKET (-1)
#endif

DEFINE_KIND(k_socket);
DEFINE_KIND(k_addr);

#define val_sock(o)		((SOCKET)(int_val)val_data(o))

static value socket_new( value udp ) {
	SOCKET s;
	val_check(udp,bool);
#ifdef _WIN32
	if( !init_done ) {
		WSAStartup(MAKEWORD(2,0),&init_data);
		init_done = true;
	}
#endif
	if( val_bool(udp) )
		s = socket(AF_INET,SOCK_DGRAM,0);
	else
		s = socket(AF_INET,SOCK_STREAM,0);
	if( s == INVALID_SOCKET )
		type_error();
	return alloc_abstract(k_socket,(value)(int_val)s);
}

static value socket_close( value o ) {
	val_check_kind(o,k_socket);
	closesocket(val_sock(o));
	val_kind(o) = NULL;
	return val_true;
}

static value socket_send_char( value o, value v ) {
	int c;
	unsigned char cc;
	val_check_kind(o,k_socket);
	val_check(v,int);
	c = val_int(v);
	if( c < 0 || c > 255 )
		type_error();
	cc = (unsigned char)c;
	if( send(val_sock(o),&cc,1,0) == SOCKET_ERROR )
		type_error();
	return val_true;
}

static value socket_send( value o, value data, value pos, value len ) {
	int p,l,dlen;
	val_check_kind(o,k_socket);
	val_check(data,string);
	val_check(pos,int);
	val_check(len,int);
	p = val_int(pos);
	l = val_int(len);
	dlen = val_strlen(data);
	if( p < 0 || l < 0 || p > dlen || p + l > dlen )
		type_error();
	dlen = send(val_sock(o), val_string(data) + p , l, 0);
	if( dlen == SOCKET_ERROR )
		type_error();
	return alloc_int(dlen);
}

static value socket_recv( value o, value data, value pos, value len ) {
	int p,l,dlen;
	val_check_kind(o,k_socket);
	val_check(data,string);
	val_check(pos,int);
	val_check(len,int);
	p = val_int(pos);
	l = val_int(len);
	dlen = val_strlen(data);
	if( p < 0 || l < 0 || p > dlen || p + l > dlen )
		type_error();
	dlen = recv(val_sock(o), val_string(data) + p , l, 0);
	if( dlen == SOCKET_ERROR )
		type_error();
	return alloc_int(dlen);
}

static value socket_recv_char( value o ) {
	unsigned char cc;
	val_check_kind(o,k_socket);
	if( recv(val_sock(o),&cc,1,0) <= 0 )
		type_error();
	return alloc_int(cc);
}


static value socket_write( value o, value data ) {
	const char *cdata;
	int datalen, slen;
	val_check_kind(o,k_socket);
	val_check(data,string);	
	cdata = val_string(data);
	datalen = val_strlen(data);
	while( datalen > 0 ) {
		slen = send(val_sock(o),cdata,datalen,0);
		if( slen == SOCKET_ERROR )
			type_error();
		cdata += slen;
		datalen -= slen;
	}
	return val_true;
}

static value socket_read( value o ) {
	buffer b;
	char buf[256];
	int len;	
	val_check_kind(o,k_socket);
	b = alloc_buffer(NULL);
	while( true ) {
		len = recv(val_sock(o),buf,256,0);
		if( len == SOCKET_ERROR )
			type_error();
		if( len == 0 )
			break;
		buffer_append_sub(b,buf,len);
	}
	return buffer_to_string(b);
}

static value host_resolve( value host ) {
	unsigned int ip;	
	val_check(host,string);
	ip = inet_addr(val_string(host));
	if( ip == INADDR_NONE ) {
		struct hostent *h = gethostbyname(val_string(host));
		if( h == 0 )
			type_error();
		ip = *((unsigned int*)h->h_addr);
	}
	return alloc_int32(ip);
}

static value host_to_string( value ip ) {
	struct in_addr i;
	val_check(ip,int32);
	*(int*)&i = val_int32(ip);
	return alloc_string( inet_ntoa(i) );
}

static value host_local() {
	char buf[256];
	if( gethostname(buf,256) == SOCKET_ERROR )
		type_error();
	return alloc_string(buf);
}

static value socket_connect( value o, value host, value port ) {
	struct sockaddr_in addr;
	val_check_kind(o,k_socket);
	val_check(host,int32);
	val_check(port,int);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(val_int(port));
	*(int*)&addr.sin_addr.s_addr = val_int32(host);
	if( connect(val_sock(o),(struct sockaddr*)&addr,sizeof(addr)) <= 0 )
		type_error();
	return val_true;
}

static value socket_listen( value o, value n ) {
	val_check_kind(o,k_socket);
	val_check(n,int);
	if( listen(val_sock(o),val_int(n)) == SOCKET_ERROR )
		type_error();
	return val_true;
}

static fd_set INVALID;

static fd_set *make_socket_array( value a, fd_set *tmp ) {
	unsigned int i;
	if( val_is_null(a) )
		return NULL;
	if( !val_is_array(a) )
		return &INVALID;
	FD_ZERO(tmp);	
	for(i=0;i<tmp->fd_count;i++) {
		value s = val_array_ptr(a)[i];
		if( !val_is_kind(s,k_socket) )
			return &INVALID;
		FD_SET(val_sock(s),tmp);
	}
	return tmp;
}

static value make_array_result( value a, fd_set *tmp ) {
	value r;
	int i, len;
	int pos = 0;
	if( tmp == NULL )
		return val_null;
	len = val_array_size(a);
	r = alloc_array(len);
	for(i=0;i<len;i++) {
		value s = val_array_ptr(a)[i];
		if( FD_ISSET(val_sock(s),tmp) )
			val_array_ptr(r)[pos++] = s;
	}
	val_set_size(r,pos);
	return r;
}

static value socket_select( value rs, value ws, value es, value timeout ) {
	struct timeval tval;
	struct timeval *tt;
	tfloat f;
	fd_set rx, wx, ex;
	fd_set *ra, *wa, *ea;
	value r;
	ra = make_socket_array(rs,&rx);
	wa = make_socket_array(ws,&wx);
	ea = make_socket_array(es,&ex);
	if( ra == &INVALID || wa == &INVALID || ea == &INVALID )
		type_error();
	if( val_is_null(timeout) )
		tt = NULL;
	else {
		val_check(timeout,number);
		f = val_number(timeout);
		tval.tv_usec = ((int)(f*1000000)) % 1000000;
		tval.tv_sec = (int)f;
		tt = &tval;
	}
	if( select(0,ra,wa,ea,tt) == SOCKET_ERROR )
		type_error();
	r = alloc_array(3);
	val_array_ptr(r)[0] = make_array_result(rs,ra);
	val_array_ptr(r)[1] = make_array_result(ws,wa);
	val_array_ptr(r)[2] = make_array_result(es,ea);
	return r;
}

static value socket_bind( value o, value host, value port ) {
	struct sockaddr_in addr;
	val_check_kind(o,k_socket);
	val_check(host,int32);
	val_check(port,int);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(val_int(port));
	*(int*)&addr.sin_addr.s_addr = val_int32(host);
	if( bind(val_sock(o),(struct sockaddr*)&addr,sizeof(addr)) == SOCKET_ERROR )
		type_error();
	return val_true;
}

static value socket_accept( value o ) {
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	SOCKET s;
	val_check_kind(o,k_socket);
	s = accept(val_sock(o),(struct sockaddr*)&addr,&addrlen);
	if( s == INVALID_SOCKET )
		type_error();
	return alloc_abstract(k_socket,(value)(int_val)s);
}

static value socket_peer( value o ) {
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	value ret;
	val_check_kind(o,k_socket);
	if( getpeername(val_sock(o),(struct sockaddr*)&addr,&addrlen) == SOCKET_ERROR )
		type_error();
	ret = alloc_array(2);
	val_array_ptr(ret)[0] = alloc_int32(*(int*)&addr.sin_addr);
	val_array_ptr(ret)[1] = alloc_int(addr.sin_port);
	return ret;
}

static value socket_host( value o ) {
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	value ret;
	val_check_kind(o,k_socket);
	if( getpeername(val_sock(o),(struct sockaddr*)&addr,&addrlen) == SOCKET_ERROR )
		type_error();
	ret = alloc_array(2);
	val_array_ptr(ret)[0] = alloc_int32(*(int*)&addr.sin_addr);
	val_array_ptr(ret)[1] = alloc_int(addr.sin_port);
	return ret;
}

DEFINE_PRIM(socket_new,0);
DEFINE_PRIM(socket_send,4);
DEFINE_PRIM(socket_send_char,2);
DEFINE_PRIM(socket_recv,4);
DEFINE_PRIM(socket_recv_char,1);
DEFINE_PRIM(socket_write,2);
DEFINE_PRIM(socket_read,1);
DEFINE_PRIM(socket_close,1);
DEFINE_PRIM(socket_connect,3);
DEFINE_PRIM(socket_listen,2);
DEFINE_PRIM(socket_select,4);
DEFINE_PRIM(socket_bind,3);
DEFINE_PRIM(socket_accept,1);
DEFINE_PRIM(socket_peer,1);
DEFINE_PRIM(socket_host,1);

DEFINE_PRIM(host_local,0);
DEFINE_PRIM(host_resolve,1);
DEFINE_PRIM(host_to_string,1);

/* ************************************************************************ */
