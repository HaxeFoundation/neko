#include <neko.h>
#ifdef _WIN32
#	include <winsock2.h>
	static bool init_done = false;
	static WSADATA init_data;
#else
#	include <sys/socket.h>
#	include <arpa/inet.h>
#	include <netdb.h>
	typedef int SOCKET;
#	define closesocket close
#	define SOCKET_ERROR (-1)
#	define INVALID_SOCKET (-1)
#endif

DEFINE_KIND(k_socket);

#define val_sock(o)		((SOCKET)val_data(o))

static value socket_set_timeout( value o, value t ) {
	int time;
	val_check_kind(o,k_socket);
	if( !val_is_int(t) )
		return val_null;
	time = val_int(t);
	setsockopt(val_sock(o),SOL_SOCKET,SO_SNDTIMEO,(char*)&time,sizeof(int));
	setsockopt(val_sock(o),SOL_SOCKET,SO_RCVTIMEO,(char*)&time,sizeof(int));
	return val_true;
}

static value socket_new() {
	value o = alloc_abstract(k_socket,(void*)INVALID_SOCKET);
	val_check_kind(o,k_socket);
#ifdef _WIN32
	if( !init_done ) {
		WSAStartup(MAKEWORD(2,0),&init_data);
		init_done = true;
	}
#endif
	val_data(o) = (void*)socket(AF_INET,SOCK_STREAM,0);
	socket_set_timeout(o,alloc_int(5000));
	return o;
}

static value socket_connect( value o, value host, value port ) {
	unsigned int ip;
	struct sockaddr_in peer;
	val_check_kind(o,k_socket);
	if( !val_is_string(host) || !val_is_int(port) )
		return val_null;
	ip = inet_addr(val_string(host));
	if( ip == INADDR_NONE ) {
		struct hostent* pHE = gethostbyname(val_string(host));
		if( pHE == 0 )
			return val_null;
		memcpy(&ip,pHE->h_addr,sizeof(ip));
	}
	peer.sin_family = AF_INET;
	peer.sin_port = htons(val_int(port));
	peer.sin_addr.s_addr = ip;
	return alloc_bool( connect(val_sock(o),(struct sockaddr*)&peer,sizeof(peer)) <= 0 );
}

static value socket_send( value o, value data ) {
	const char *cdata;
	int datalen, slen;
	val_check_kind(o,k_socket);
	if( !val_is_string(data) )
		return val_null;	
	cdata = val_string(data);
	datalen = val_strlen(data);
	while( datalen > 0 && (slen = send(val_sock(o),cdata,datalen,0)) != SOCKET_ERROR ) {
		cdata += slen;
		datalen -= slen;
	}
	return alloc_bool( slen != SOCKET_ERROR );
}

static value socket_receive( value o ) {
	value s;
	buffer b;
	char buf[256];
	int len;	
	val_check_kind(o,k_socket);
	b = alloc_buffer(NULL);
	while( (len = recv(val_sock(o),buf,256,0)) != SOCKET_ERROR && len > 0 )
		buffer_append_sub(b,buf,len);	
	s = buffer_to_string(b);
	if( val_strlen(s) == 0 )
		return val_null;
	return s;
}

static value socket_close( value o ) {
	val_check_kind(o,k_socket);
	closesocket(val_sock(o));
	val_data(o) = (void*)INVALID_SOCKET;
	return val_true;
}

DEFINE_PRIM(socket_new,0);
DEFINE_PRIM(socket_connect,3);
DEFINE_PRIM(socket_send,2);
DEFINE_PRIM(socket_receive,1);
DEFINE_PRIM(socket_close,1);
DEFINE_PRIM(socket_set_timeout,2);
