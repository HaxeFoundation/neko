
#define IMPLEMENT_API
#define NEKO_COMPATIBLE

#include <neko.h>
#include <stdio.h>
#include <string.h>

#if !_MSC_VER
#include <sys/socket.h>
#include <strings.h>
#endif

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/asn1.h>

#ifdef _MSC_VER
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#endif

#include <openssl/x509v3.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#if !_MSC_VER
typedef int SOCKET;
#endif

#define SOCKET_ERROR (-1)
#define NRETRYS	20

#define val_ssl(o)	(SSL*)val_data(o)
#define val_ctx(o)	(SSL_CTX*)val_data(o)
#define val_x509(o) (X509*)val_data(o)
#define val_pkey(o) (EVP_PKEY*)val_data(o)
#define alloc_null() val_null;

DEFINE_KIND( k_ssl_method );
DEFINE_KIND( k_ssl_ctx );
DEFINE_KIND( k_ssl );
DEFINE_KIND( k_bio );
DEFINE_KIND( k_x509 );
DEFINE_KIND( k_pkey );

static vkind k_socket;

static void free_x509( value v ){
	X509_free(val_x509(v));
}

static void free_pkey( value v ){
	EVP_PKEY_free(val_pkey(v));
}

#if !_MSC_VER
typedef struct {
	SSL *ssl;
	char *buf;
	int size;
	int ret;
} sock_tmp;
#endif

typedef enum {
	MatchFound,
	MatchNotFound,
	NoSANPresent,
	MalformedCertificate,
	Error
} HostnameValidationResult;

static value block_error() {
	#ifdef NEKO_WINDOWS
	int err = WSAGetLastError();
	if( err == WSAEWOULDBLOCK || err == WSAEALREADY || err == WSAETIMEDOUT )
	#else
	if( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EALREADY )
	#endif
		val_throw(alloc_string("Blocking"));
	neko_error();
	return val_true;
}

static value ssl_error( SSL *ssl, int ret ) {
	int err = SSL_get_error(ssl, ret);
	switch( err ){
		case SSL_ERROR_WANT_READ:
			val_throw(alloc_string("SSL_ERROR_WANT_READ"));
		break;
		case SSL_ERROR_WANT_WRITE:
			val_throw(alloc_string("SSL_ERROR_WANT_WRITE"));
		break;
		case SSL_ERROR_WANT_CONNECT:
			val_throw(alloc_string("SSL_ERROR_WANT_CONNECT"));
		break;
		case SSL_ERROR_WANT_ACCEPT:
			val_throw(alloc_string("SSL_ERROR_WANT_ACCEPT"));
		break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			val_throw(alloc_string("SSL_ERROR_WANT_X509_LOOKUP"));
		break;
		case SSL_ERROR_SYSCALL:
			val_throw(alloc_string("SSL_ERROR_SYSCALL"));
		break;
		case SSL_ERROR_SSL:
			val_throw(alloc_string(ERR_reason_error_string(ERR_get_error())));
		break;
	}
	neko_error();
}

static value bio_noclose() {
	return alloc_int( BIO_NOCLOSE );
}

static value bio_new_socket( value sock, value close_flag ) {
	BIO* bio;
	if( !k_socket ) k_socket = kind_lookup("socket");
	val_check_kind(sock, k_socket);
	bio = BIO_new_socket( ((int_val) val_data(sock)), val_int(close_flag) );
	return alloc_abstract( k_bio, bio );
}

static value client_method() {
	return alloc_abstract( k_ssl_method, (SSL_METHOD*)SSLv23_client_method() );
}

static value server_method() {
    return alloc_abstract( k_ssl_method, (SSL_METHOD*)SSLv23_server_method() );
}

static value ssl_new( value ctx ) {
	SSL* ssl;
	val_check_kind(ctx,k_ssl_ctx);
	ssl = SSL_new( val_ctx(ctx) );
	if( ssl == NULL )
		neko_error();
	return alloc_abstract( k_ssl, ssl );
}

static value ssl_close( value ssl ) {
	val_check_kind(ssl,k_ssl);
	SSL_free( val_ssl(ssl) );
	val_kind(ssl) = NULL;
	return val_true;
}

static value ssl_connect( value ssl ) {
	int r;
	val_check_kind(ssl,k_ssl);
	r = SSL_connect( val_ssl(ssl) );
	if( r < 0 )
		ssl_error(val_ssl(ssl),r);
	return val_true;
}

static value ssl_shutdown( value ssl ) {
	val_check_kind(ssl,k_ssl);
	return alloc_int( SSL_shutdown( val_ssl(ssl) ) );
}

static value ssl_free( value ssl ) {
	val_check_kind(ssl,k_ssl);
	SSL_free( val_ssl(ssl) );
	return val_true;
}

static value ssl_set_bio( value ssl, value rbio, value wbio ) {
	val_check_kind(ssl,k_ssl);
	val_check_kind(rbio,k_bio);
	val_check_kind(wbio,k_bio);
	SSL_set_bio( val_ssl(ssl), (BIO*) val_data(rbio), (BIO*) val_data(wbio) );
	return val_true;
}

static value ssl_set_hostname( value ssl, value hostname ){
	val_check_kind(ssl,k_ssl);
	val_check(hostname,string);
	if( !SSL_set_tlsext_host_name( val_ssl(ssl), val_string(hostname) ) )
		neko_error();
	return val_true;
}

static value ssl_accept( value ssl ) {
	int r;
	SSL *_ssl;
	val_check_kind(ssl,k_ssl);
	_ssl = val_ssl( ssl );
	r = SSL_accept( _ssl );
	if( r < 0 )
	    ssl_error(_ssl,r);
	return val_true;
}

static value ssl_get_peer_certificate( value ssl ){
	X509 *cert;
	val_check_kind(ssl,k_ssl);
	cert = SSL_get_peer_certificate(val_ssl(ssl));
	return cert ? alloc_abstract( k_x509, cert ) : val_null;
}

// HostnameValidation based on https://github.com/iSECPartners/ssl-conservatory
// Wildcard cmp based on libcurl hostcheck
static HostnameValidationResult match_hostname( const ASN1_STRING *asn1, const char *hostname ){
	char *pattern, *wildcard, *pattern_end, *hostname_end;
	int prefixlen, suffixlen;
	if( asn1 == NULL )
		return Error;

	pattern = (char *)ASN1_STRING_data((ASN1_STRING *)asn1);

	if( ASN1_STRING_length((ASN1_STRING *)asn1) != strlen(pattern) ){
		return MalformedCertificate;
	}else{
		wildcard = strchr(pattern, '*');
		if( wildcard == NULL ){
			if( strcasecmp(pattern, hostname) == 0 )
				return MatchFound;
			return MatchNotFound;
		}
		pattern_end = strchr(pattern, '.');
		if( pattern_end == NULL || strchr(pattern_end+1,'.') == NULL || wildcard > pattern_end || strncasecmp(pattern,"xn--",4)==0 )
			return MatchNotFound;
		hostname_end = strchr(hostname, '.');
		if( hostname_end == NULL || strcasecmp(pattern_end, hostname_end) != 0 )
			return MatchNotFound;
		if( hostname_end-hostname < pattern_end-pattern )
			return MatchNotFound;

		prefixlen = wildcard - pattern;
		suffixlen = pattern_end - (wildcard+1);
		if( strncasecmp(pattern, hostname, prefixlen) != 0 )
			return MatchNotFound;
		if( strncasecmp(pattern_end-suffixlen, hostname_end-suffixlen, suffixlen) != 0 )
			return MatchNotFound;

		return MatchFound;
	}
	return MatchNotFound;
}

static HostnameValidationResult matches_subject_alternative_name( const X509 *server_cert, const char *hostname ){
	int i, nb = -1;
	HostnameValidationResult r;
	STACK_OF(GENERAL_NAME) *san_names = NULL;

	san_names = (STACK_OF(GENERAL_NAME) *) X509_get_ext_d2i((X509 *)server_cert, NID_subject_alt_name, NULL, NULL);
	if( san_names == NULL )
		return NoSANPresent;
	nb = sk_GENERAL_NAME_num(san_names);

	for( i=0; i<nb; i++ ){
		const GENERAL_NAME *cur = sk_GENERAL_NAME_value(san_names, i);
		if( cur->type == GEN_DNS ){
			r = match_hostname( cur->d.dNSName, hostname );
			if( r != MatchNotFound )
				return r;
		}
	}
	sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);
	return MatchNotFound;
}

static HostnameValidationResult matches_common_name(const X509 *server_cert, const char *hostname ){
	int cn_loc = -1;
	X509_NAME_ENTRY *cn_entry = NULL;

	cn_loc = X509_NAME_get_index_by_NID(X509_get_subject_name((X509 *)server_cert), NID_commonName, -1);
	if( cn_loc < 0 )
		return Error;

	cn_entry = X509_NAME_get_entry(X509_get_subject_name((X509 *)server_cert), cn_loc);
	if( cn_entry == NULL )
		return Error;

	return match_hostname(X509_NAME_ENTRY_get_data(cn_entry), hostname);
}

static value x509_validate_hostname( value cert, value hostname ){
	X509 *server_cert;
	const char *name;
	HostnameValidationResult result;
	val_check_kind(cert,k_x509);
	val_check(hostname,string);
	name = val_string(hostname);
	server_cert = val_x509(cert);
	if( server_cert == NULL )
		neko_error();

	result = matches_subject_alternative_name(server_cert, name);
	if (result == NoSANPresent)
		result = matches_common_name(server_cert, name);

	if( result == MatchFound )
		return val_true;

	switch( result ){
		case MatchNotFound:
			return val_false;
			break;
		case MalformedCertificate:
			return val_false;
			break;
		default:
			break;
	}

	neko_error();
}

static value ssl_send_char( value ssl, value v ) {
	unsigned char cc;
	int c;
	val_check_kind(ssl,k_ssl);
	val_check(v,int);
	c = val_int(v);
	if( c < 0 || c > 255 )
		neko_error();
	cc = (unsigned char) c;
	SSL_write( val_ssl(ssl), &cc, 1 );
	return val_true;
}

static value ssl_send( value ssl, value data, value pos, value len ) {
	int p,l,dlen;
	val_check_kind(ssl,k_ssl);
	val_check(data,string);
	val_check(pos,int);
	val_check(len,int);
	p = val_int(pos);
	l = val_int(len);
	dlen = val_strlen(data);
	if( p < 0 || l < 0 || p > dlen || p + l > dlen )
		neko_error();
	POSIX_LABEL(send_again);
	dlen = SSL_write( val_ssl(ssl), val_string(data) + p, l );
	if( dlen == SOCKET_ERROR ) {
		HANDLE_EINTR(send_again);
		return block_error();
	}
	return alloc_int(dlen);
}

static value ssl_write( value ssl, value data ) {
	int len, slen;
	const char *s;
	SSL *_ssl;
	val_check_kind(ssl,k_ssl);
	val_check(data,string);
	s = val_string( data );
	len = val_strlen( data );
	_ssl = val_ssl(ssl);
	while( len > 0 ) {
		POSIX_LABEL( write_again );
		slen = SSL_write( _ssl, s, len );
		if( slen == SOCKET_ERROR ) {
			HANDLE_EINTR( write_again );
			return block_error();
		}
		s += slen;
		len -= slen;
	}
	return val_true;
}

static value ssl_recv_char(value ssl) {
	unsigned char c;
	int r;
	val_check_kind(ssl,k_ssl);
	r = SSL_read( val_ssl(ssl), &c, 1 );
	if( r <= 0 )
		neko_error();
	return alloc_int( c );
}

static value ssl_recv( value ssl, value data, value pos, value len ) {
	int p,l,dlen;
	void * buf;
	val_check_kind(ssl,k_ssl);
	val_check(data,string);
	val_check(pos,int);
	val_check(len,int);
	p = val_int( pos );
	l = val_int( len );
	buf = (void *) (val_string(data) + p);
	POSIX_LABEL(recv_again);
	dlen = SSL_read( val_ssl(ssl), buf, l );
	if( dlen == SOCKET_ERROR ) {
		HANDLE_EINTR(recv_again);
		return block_error();
	}
	if( dlen < 0 )
		neko_error();
	return alloc_int( dlen );
}

static  value ssl_read( value ssl ) {
	int len, bufsize = 256;
	buffer b;
	char buf[256];
	SSL* _ssl;
	val_check_kind(ssl,k_ssl);
	_ssl = val_ssl(ssl);
	b = alloc_buffer(NULL);
	while( true ) {
		POSIX_LABEL(read_again);
		len = SSL_read( _ssl, buf, bufsize );
		if( len == SOCKET_ERROR ) {
			HANDLE_EINTR(read_again);
			return block_error();
		}
		if( len == 0 )
			break;
		buffer_append_sub(b,buf,len);
	}
	return buffer_to_string(b);
}

static value ctx_new( value m ) {
	SSL_CTX *ctx;
	val_check_kind(m,k_ssl_method);
	ctx = SSL_CTX_new( (SSL_METHOD*) val_data(m) );
	SSL_CTX_set_options( ctx, SSL_OP_NO_SSLv2 );
	SSL_CTX_set_options( ctx, SSL_OP_NO_SSLv3 );
	return alloc_abstract( k_ssl_ctx, ctx );
}
static value ctx_close( value ctx ) {
	val_check_kind(ctx,k_ssl_ctx);
	SSL_CTX_free( val_ctx(ctx) );
	val_kind(ctx) = NULL;
	return val_true;
}

static value ctx_set_cipher_list( value ctx, value str, value preferServer ){
	SSL_CTX *_ctx;
	val_check_kind(ctx,k_ssl_ctx);
	if( !val_is_null(str) ) val_check(str,string);
	val_check(preferServer,bool);
	_ctx = val_ctx(ctx);

	if( !val_is_null(str) ){
		if( !SSL_CTX_set_cipher_list( _ctx, val_string(str) ) )
			neko_error();
	}

	if( val_bool(preferServer) )
		SSL_CTX_set_options( _ctx, SSL_OP_CIPHER_SERVER_PREFERENCE );
	return val_true;
}

static value ctx_set_ecdh( value ctx ){
	SSL_CTX *_ctx;
	EC_KEY *eckey;
	val_check_kind(ctx,k_ssl_ctx);
	_ctx = val_ctx(ctx);
    eckey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    SSL_CTX_set_tmp_ecdh(_ctx, eckey);
	SSL_CTX_set_options(_ctx, SSL_OP_SINGLE_ECDH_USE);
    EC_KEY_free(eckey);
	return val_true;
}

static value ctx_set_dhfile( value ctx, value file ){
	DH   *dh;
    BIO  *bio;
	SSL_CTX *_ctx;

	val_check_kind(ctx,k_ssl_ctx);
	val_check(file,string);

    _ctx = val_ctx(ctx);
	bio = BIO_new_file(val_string(file), "r");

    if (bio == NULL)
		neko_error();

    dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    if (dh == NULL) {
        BIO_free(bio);
        neko_error();
    }

    if( SSL_CTX_set_tmp_dh(_ctx, dh) != 1 )
		neko_error();

	SSL_CTX_set_options(_ctx, SSL_OP_SINGLE_DH_USE);

    DH_free(dh);
    BIO_free(bio);
	return val_true;
}

static value ctx_load_verify_locations( value ctx, value certFile, value certFolder ) {
	val_check_kind(ctx,k_ssl_ctx);
	if( val_is_null( certFile ) && val_is_null( certFolder ) ) {
		if( !SSL_CTX_set_default_verify_paths( val_ctx(ctx) ) )
			neko_error();
	} else {
		val_check(certFile,string);
		val_check(certFolder,string);
		if( !SSL_CTX_load_verify_locations( val_ctx(ctx), val_string(certFile), val_string(certFolder) ) )
			neko_error();
	}
	return val_true;
}

static value ctx_set_verify( value ctx, value b ) {
	val_check_kind(ctx, k_ssl_ctx);
	val_check(b, bool);
	if (val_bool(b))
		SSL_CTX_set_verify(val_ctx(ctx), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	else
		SSL_CTX_set_verify(val_ctx(ctx), SSL_VERIFY_NONE, NULL);
	return val_true;
}

static value ctx_use_certificate_file( value ctx, value certFile, value privateKeyFile ) {
	SSL_CTX* _ctx;
	val_check_kind(ctx,k_ssl_ctx);
	val_check(certFile,string);
	val_check(privateKeyFile,string);

	_ctx = val_ctx(ctx);
	if( SSL_CTX_use_certificate_chain_file( _ctx, val_string(certFile) ) <= 0 ){
		val_throw(alloc_string("SSL_CTX_use_certificate_chain_file"));
	}

	if( SSL_CTX_use_PrivateKey_file( _ctx, val_string(privateKeyFile), SSL_FILETYPE_PEM ) <= 0 ){
		val_throw(alloc_string("SSL_CTX_use_PrivateKey_file"));
	}

	if( !SSL_CTX_check_private_key(_ctx) )
 		neko_error();
	return val_true;
}

static value ctx_set_session_id_context( value ctx, value sid ) {
	val_check_kind(ctx,k_ssl_ctx);
	val_check(sid,string);
	if (SSL_CTX_set_session_id_context(val_ctx(ctx), (unsigned char *)val_string(sid), val_strlen(sid)) != 1)
		neko_error();
	return val_true;
}

static int servername_cb(SSL *ssl, int *ad, void *arg){
	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if( servername && arg ){
	 	value ret = val_call1((value)arg, alloc_string(servername)) ;
		if( !val_is_null(ret) )
			SSL_set_SSL_CTX( ssl, val_ctx(ret) );
	}
	return SSL_TLSEXT_ERR_OK;
}

static value ctx_set_servername_callback( value ctx, value cb ){
	SSL_CTX *_ctx;
	val_check_kind(ctx,k_ssl_ctx);
	val_check_function(cb,1);
	_ctx = val_ctx(ctx);
	SSL_CTX_set_tlsext_servername_callback( _ctx, servername_cb );
	SSL_CTX_set_tlsext_servername_arg( _ctx, (void *)cb );
	return val_true;
}

static value x509_load_file(value file){
	BIO *in;
	X509 *x = NULL;
	value v;
	val_check(file,string);
	in = BIO_new(BIO_s_file());
	if (in == NULL)
		neko_error();
	if (BIO_read_filename(in, val_string(file)) <= 0){
		BIO_free(in);
		neko_error();
	}
	x = PEM_read_bio_X509(in, NULL, NULL, NULL);
	BIO_free(in);
	if (x == NULL)
		neko_error();
	v = alloc_abstract(k_x509, x);
	val_gc(v,free_x509);
	return v;
}

static value x509_get_subject( value cert, value objname ){
	int loc = -1;
	X509_NAME *subject;
	ASN1_OBJECT *obj;
	const char *str;
	val_check_kind(cert,k_x509);
	val_check(objname, string);
	obj = OBJ_txt2obj(val_string(objname), 0);
	if (!obj)
		neko_error();
	subject = X509_get_subject_name(val_x509(cert));
	if (!subject)
		return val_null;
	loc = X509_NAME_get_index_by_OBJ(subject, obj, -1);
	if (loc < 0)
		return val_null;
	str = (char*)ASN1_STRING_data(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subject, loc)));
	return alloc_string(str);
}

static value x509_get_issuer(value cert, value objname){
	int loc = -1;
	X509_NAME *issuer;
	ASN1_OBJECT *obj;
	const char *str;
	val_check_kind(cert, k_x509);
	val_check(objname, string);
	obj = OBJ_txt2obj(val_string(objname), 0);
	if (!obj)
		neko_error();
	issuer = X509_get_issuer_name(val_x509(cert));
	if (!issuer)
		return val_null;
	loc = X509_NAME_get_index_by_OBJ(issuer, obj, -1);
	if (loc < 0)
		return val_null;
	str = (char *)ASN1_STRING_data(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(issuer, loc)));
	return alloc_string(str);
}


static value x509_get_altnames( value cert ){
	int i, nb = -1;
	STACK_OF(GENERAL_NAME) *san_names = NULL;
	value l, first;
	val_check_kind(cert, k_x509);
	san_names = (STACK_OF(GENERAL_NAME) *) X509_get_ext_d2i(val_x509(cert), NID_subject_alt_name, NULL, NULL);
	if (san_names == NULL)
		return val_null;
	nb = sk_GENERAL_NAME_num(san_names);
	l = NULL;
	first = NULL;
	for (i = 0; i < nb; i++){
		const GENERAL_NAME *cur = sk_GENERAL_NAME_value(san_names, i);
		if (cur->type == GEN_DNS){
			value l2 = alloc_array(2);
			val_array_ptr(l2)[0] = alloc_string((char*)ASN1_STRING_data(cur->d.dNSName));
			val_array_ptr(l2)[1] = val_null;
			if (first == NULL)
				first = l2;
			else
				val_array_ptr(l)[1] = l2;
			l = l2;
		}
	}
	return (first==NULL)?val_null:first;
}

static value x509_cmp_notbefore(value cert, value date){
	ASN1_TIME *t;
	time_t d;
	val_check_kind(cert, k_x509);
	val_check(date, any_int);
	d = val_any_int(date);
	t = X509_get_notBefore(val_x509(cert));
	return alloc_int(X509_cmp_time(t,&d));
}

static value x509_cmp_notafter(value cert, value date){
	ASN1_TIME *t;
	time_t d;
	val_check_kind(cert, k_x509);
	val_check(date, any_int);
	d = val_any_int(date);
	t = X509_get_notAfter(val_x509(cert));
	return alloc_int(X509_cmp_time(t,&d));
}

int password_callback(char *buf, int bufsiz, int verify, void *pass){
	int res = 0;
	if (pass){
		res = strlen((char *)pass);
		if (res > bufsiz)
			res = bufsiz;
		memcpy(buf, pass, res);
	}
	return res;
}

static value key_from_pem(value data, value pub, value pass){
	BIO *key = NULL;
	EVP_PKEY *pkey = NULL;
	value v;
	val_check(data,string);
	val_check(pub, bool);
	if (!val_is_null(pass)) val_check(pass, string);

	key = BIO_new_mem_buf((void *)val_string(data), val_strlen(data));
	if (val_bool(pub))
		pkey = PEM_read_bio_PUBKEY(key, NULL, password_callback, val_is_null(pass) ? NULL : (void *)val_string(pass));
	else
		pkey = PEM_read_bio_PrivateKey(key, NULL, password_callback, val_is_null(pass) ? NULL : (void *)val_string(pass));
	BIO_free(key);
	if (pkey == NULL)
		neko_error();
	v = alloc_abstract(k_pkey, pkey);
	val_gc(v,free_pkey);
	return v;
}

static value key_from_der(value data, value pub){
	BIO *key = NULL;
	EVP_PKEY *pkey = NULL;
	value v;
	val_check(data, string);
	val_check(pub, bool);

	key = BIO_new_mem_buf((void *)val_string(data), val_strlen(data));
	if (val_bool(pub))
		pkey = d2i_PUBKEY_bio(key, NULL);
	else
		pkey = d2i_PrivateKey_bio(key, NULL);
	BIO_free(key);
	if (pkey == NULL)
		neko_error();
	v = alloc_abstract(k_pkey, pkey);
	val_gc(v,free_pkey);
	return v;
}

static value dgst_sign(value data, value key, value alg){
	BIO *in = NULL;
	BIO *bmd = NULL;
	const EVP_MD *md = NULL;
	EVP_MD_CTX *mctx = NULL;
	EVP_MD_CTX *ctx;
	EVP_PKEY *sigkey = NULL;
	size_t bufsize = 1024 * 8, len;
	bool success = false;
	int i;
	value out;
	char *buf;	
	val_check(data, string);
	val_check_kind(key, k_pkey);
	val_check(alg, string);
	
	md = EVP_get_digestbyname(val_string(alg));
	if( md == NULL )
		goto end;
	in = BIO_new_mem_buf((void *)val_string(data), val_strlen(data));
	if( in == NULL )
		goto end;
	bmd = BIO_new(BIO_f_md());
	if( bmd == NULL )
		goto end;
	if (!BIO_get_md_ctx(bmd, &mctx)) 
		goto end;
	sigkey = val_pkey(key);
	if ( !EVP_DigestSignInit(mctx, NULL, md, NULL, sigkey) )
		goto end;
	bmd = BIO_push(bmd, in);
	out = alloc_empty_string(bufsize);
	buf = val_string(out);
	for (;;) {
		i = BIO_read(bmd, (char *)buf, bufsize);
		if (i < 0)
			goto end;
		if (i == 0)
			break;
	}

	BIO_get_md_ctx(bmd, &ctx);
	len = bufsize;
	if (EVP_DigestSignFinal(ctx, (unsigned char *)buf, &len)){
		success = true;
		buf[len] = 0;
		val_set_size(out, len);
	}

end:
	if (in != NULL)
		BIO_free(in);
	if (bmd != NULL)
		BIO_free(bmd);
	if (success)
		return out;
	else
		neko_error();
}

static value dgst_verify( value data, value sign, value key, value alg ){
	BIO *in = NULL;
	BIO *bmd = NULL;
	const EVP_MD *md = NULL;
	EVP_MD_CTX *mctx = NULL;
	EVP_MD_CTX *ctx;
	EVP_PKEY *sigkey = NULL;
	size_t bufsize = 1024 * 8;
	bool result = -1;
	int i;
	char *buf;
	val_check(data, string);
	val_check(sign, string);
	val_check_kind(key, k_pkey);
	val_check(alg, string);

	md = EVP_get_digestbyname(val_string(alg));
	if (md == NULL)
		goto end;
	in = BIO_new_mem_buf((void *)val_string(data), val_strlen(data));
	if (in == NULL)
		goto end;
	bmd = BIO_new(BIO_f_md());
	if (bmd == NULL)
		goto end;
	if (!BIO_get_md_ctx(bmd, &mctx))
		goto end;
	sigkey = val_pkey(key);
	if (!EVP_DigestVerifyInit(mctx, NULL, md, NULL, sigkey))
		goto end;

	bmd = BIO_push(bmd, in);
	buf = malloc( bufsize );
	for (;;) {
		i = BIO_read(bmd, (char *)buf, bufsize);
		if (i < 0)
			goto end;
		if (i == 0)
			break;
	}

	BIO_get_md_ctx(bmd, &ctx);
	result = EVP_DigestVerifyFinal(ctx, (unsigned char*)val_string(sign), val_strlen(sign));

end:
	if (in != NULL)
		BIO_free(in);
	if (bmd != NULL)
		BIO_free(bmd);
	if (buf != NULL)
		free(buf);

	if (result < 0)
		neko_error();
	else
		return result > 0 ? val_true : val_false;
}

void ssl_main() {
	SSL_library_init();
	SSL_load_error_strings();
}

DEFINE_PRIM( bio_noclose, 0 );
DEFINE_PRIM( bio_new_socket, 2 );

DEFINE_PRIM( client_method, 0 );
DEFINE_PRIM( server_method, 0 );

DEFINE_PRIM( ssl_new, 1 );
DEFINE_PRIM( ssl_close, 1 );
DEFINE_PRIM( ssl_connect, 1 );
DEFINE_PRIM( ssl_shutdown, 1 );
DEFINE_PRIM( ssl_free, 1 );
DEFINE_PRIM( ssl_set_bio, 3 );
DEFINE_PRIM( ssl_set_hostname, 2 );
DEFINE_PRIM( ssl_accept, 1 );
DEFINE_PRIM( ssl_get_peer_certificate, 1 );

DEFINE_PRIM( ssl_send_char, 2 );
DEFINE_PRIM( ssl_send, 4 );
DEFINE_PRIM( ssl_write, 2 );
DEFINE_PRIM( ssl_recv_char, 1 );
DEFINE_PRIM( ssl_recv, 4 );
DEFINE_PRIM( ssl_read, 1 );

DEFINE_PRIM( ctx_new, 1 );
DEFINE_PRIM( ctx_close, 1 );
DEFINE_PRIM( ctx_set_cipher_list, 3 );
DEFINE_PRIM( ctx_set_ecdh, 1 );
DEFINE_PRIM( ctx_set_dhfile, 2 );
DEFINE_PRIM( ctx_load_verify_locations, 3 );
DEFINE_PRIM( ctx_set_verify, 2 );
DEFINE_PRIM( ctx_use_certificate_file, 3 );
DEFINE_PRIM( ctx_set_session_id_context, 2 );
DEFINE_PRIM( ctx_set_servername_callback, 2 );

DEFINE_PRIM( x509_load_file, 1 );
DEFINE_PRIM( x509_get_subject, 2 );
DEFINE_PRIM( x509_get_issuer, 2 );
DEFINE_PRIM( x509_get_altnames, 1 );
DEFINE_PRIM( x509_cmp_notbefore, 2 );
DEFINE_PRIM( x509_cmp_notafter, 2 );
DEFINE_PRIM( x509_validate_hostname, 2 );

DEFINE_PRIM( key_from_pem, 3 );
DEFINE_PRIM( key_from_der, 2 );

DEFINE_PRIM( dgst_sign, 3 );
DEFINE_PRIM( dgst_verify, 4 );

DEFINE_ENTRY_POINT(ssl_main);
