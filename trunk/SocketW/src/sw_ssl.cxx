// C++ Socket Wrapper
// SocketW SSL socket
//
// Started 020316
//
// License: LGPL v2.1+ (see the file LICENSE)
// (c)2002-2003 Anders Lindström

/***********************************************************************
 *  This library is free software; you can redistribute it and/or      *
 *  modify it under the terms of the GNU Lesser General Public         *
 *  License as published by the Free Software Foundation; either       *
 *  version 2.1 of the License, or (at your option) any later version. *
 ***********************************************************************/

#include "sw_ssl.h"

#ifdef _HAVE_SSL

#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

using namespace std;

#ifdef __WIN32__
//Defined in sw_base.cxx
extern int close(int fd);
extern int fcntl(int fd, int cmd, long arg);
#endif


//====================================================================
//== Helper functions
//====================================================================
RSA* generate_rsakey(int len, int exp=RSA_KEYEXP) 
{
	return RSA_generate_key(len,exp,NULL,NULL);
}

EVP_PKEY* generate_pkey(RSA *rsakey) 
{
	EVP_PKEY *pkey=NULL;

	if( !(pkey=EVP_PKEY_new()) )
		return NULL;
	
	if (!EVP_PKEY_assign_RSA(pkey, rsakey)){
		EVP_PKEY_free(pkey);
		return NULL;
	}
	
	return(pkey);
}

X509* BuildCertificate(char *name, char *organization, char *country, EVP_PKEY *key) 
{
	if( !name )
		return NULL;  //Atleast a name should be provided
	
	/* Create an X509_NAME structure to hold the distinguished name */
	X509_NAME *n = X509_NAME_new();
	if( !n )
		return NULL;
	
	// Add fields
	if ( !X509_NAME_add_entry_by_NID(n, NID_commonName, MBSTRING_ASC, (unsigned char*)name, -1, -1, 0) ){
		X509_NAME_free(n);
		return NULL;
	}
	if( organization ){
		if ( !X509_NAME_add_entry_by_NID(n, NID_organizationName, MBSTRING_ASC, (unsigned char*)organization, -1, -1, 0) ){
			X509_NAME_free(n);
			return NULL;
		}
	}
	if( country ){
		if ( !X509_NAME_add_entry_by_NID(n, NID_countryName, MBSTRING_ASC, (unsigned char*)country, -1, -1, 0) ){
			X509_NAME_free(n);
			return NULL;
		}
	}
	
	
	X509 *c = X509_new();
	if( !c ){
		X509_NAME_free(n);
		return NULL;
	}

	/* Set subject and issuer names to the X509_NAME we made */
	X509_set_issuer_name(c, n);
	X509_set_subject_name(c, n);
	X509_NAME_free(n);

	/* Set serial number to zero */
	ASN1_INTEGER_set(X509_get_serialNumber(c), 0);

	/* Set the valid/expiration times */
	ASN1_UTCTIME *s = ASN1_UTCTIME_new();
	if( !s ){
		X509_free(c);
		return NULL;
	}
	
	X509_gmtime_adj(s, -60*60*24);
	X509_set_notBefore(c, s);
	X509_gmtime_adj(s, 60*60*24*364);
	X509_set_notAfter(c, s);
	
	ASN1_UTCTIME_free(s);

	/* Set the public key */
	X509_set_pubkey(c, key);

	/* Self-sign it */
	X509_sign(c, key, EVP_sha1());

	return(c);
}

// Password callback
int pem_passwd_cb(char *buf, int size, int rwflag, void *password)
{
	strncpy(buf, (char *)(password), size);
	buf[size - 1] = '\0';
	
	return(strlen(buf));
}

// Verify callback
int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	// We don't care. Continue with handshake

	return 1; /* Accept connection */
}

//====================================================================
//== SSL Error class
//====================================================================
SWSSLSocket::SWSSLError::SWSSLError()
{
	se = noSSLError;
}
		
SWSSLSocket::SWSSLError::SWSSLError(ssl_error e):
  SWBaseError(fatal)
{
	se = e;
}
		
SWSSLSocket::SWSSLError::SWSSLError(base_error e):
  SWBaseError(e)
{
	se = noSSLError;
}

bool SWSSLSocket::SWSSLError::operator==(SWSSLError e)
{
	if( se == noSSLError )
		return be == e.be;
			
	return se == e.se;
}
		
bool SWSSLSocket::SWSSLError::operator!=(SWSSLError e)
{
	if( se == noSSLError )
		return be != e.be;
				
	return se != e.se;
}


//====================================================================
//== SWSSLSocket
//== SSL (TCP/IP) streaming sockets
//====================================================================
SWSSLSocket::SWSSLSocket(block_type block, int keysize):
  SWInetSocket(block)
{
	static bool firstuse = true;

	//kick the SSL engine awake
	if( firstuse ){
		SSL_load_error_strings();
		SSL_library_init();
		
		
		// Give OpenSSL some random data
		// FIXME: Use a better random source than rand()
		
		// seed with time()
		time_t seed = time(NULL);
		srand(seed);
        int tmp;
		
		// Seed PRNG if needed
		while( RAND_status() == 0 ){
			//PRNG may need lots of seed data
			tmp = rand();
			RAND_seed(&tmp, sizeof(int));
		}
		
		firstuse = false;
	}
	
	// Init data
	ctx = NULL;
	ssl = NULL;
	
	have_cert = false;
	
	rsa_keysize = keysize;
	
	ud = NULL;
}

SWSSLSocket::~SWSSLSocket()
{
	if( ssl )
		SSL_free(ssl);

	if( ctx )
		SSL_CTX_free(ctx);
		
	if( ud )
		delete[] ud;
}

bool SWSSLSocket::use_cert(const char *cert_file, const char *private_key_file, SWBaseError *error)
{
	return use_cert_cb(cert_file, private_key_file, NULL, NULL, error);
}

bool SWSSLSocket::use_cert_passwd(const char *cert_file, const char *private_key_file, std::string passwd, SWBaseError *error)
{
	if( ud ){
		delete[] ud;
		ud = NULL;
	}
	
	ud = new char[passwd.size() + 1];
	
	strncpy(ud, passwd.c_str(), passwd.size() + 1);
	
	return use_cert_cb(cert_file, private_key_file, &pem_passwd_cb, ud, error);
}

bool SWSSLSocket::use_cert_cb(const char *cert_file, const char *private_key_file, int passwd_cb(char *buf, int size, int rwflag, void *userdata), char *userdata, SWBaseError *error)
{
	if( !cert_file || !private_key_file ){
		set_SSLError(error, fatal, "SWSSLSocket::use_cert_cb() - Invalid argument");
		return false;
	}
	
	if( !check_ctx(error) )
		return false;
		
	have_cert = false;
	
	// Load CERT PEM file
	if( !SSL_CTX_use_certificate_chain_file(ctx, cert_file) ){
		handle_ERRerror(error, badFile, "SWSSLSocket::use_cert_cb() on file " + string(cert_file) + " ");
		return false;	
	}
	
	// Load private key PEM file
	// Give passwd callback if any
	if( passwd_cb )
		SSL_CTX_set_default_passwd_cb(ctx, passwd_cb);
	if( userdata )
		SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)userdata);
		
	for( int i=0; i<3; i++ ){
		if( SSL_CTX_use_PrivateKey_file(ctx, private_key_file, SSL_FILETYPE_PEM) )
			break;
			
		if( ERR_GET_REASON(ERR_peek_error())==EVP_R_BAD_DECRYPT ){
			/* Give the user two tries */
			if( i<2 ){
               	ERR_get_error(); //remove from stack
               	continue;
            }
				
			handle_ERRerror(error, badPasswd, "SWSSLSocket::use_cert_cb() on file " + string(private_key_file) + " ");
			return false;
		}
			
		handle_ERRerror(error, badFile, "SWSSLSocket::use_cert_cb() on file " + string(private_key_file) + " ");
		return false;
	}
	
	// Check private key
	if( !SSL_CTX_check_private_key(ctx) ){
		handle_ERRerror(error, fatal, "SWSSLSocket::use_cert_cb() ");
		return false;	
	}
	
	have_cert = true;
	
	no_error(error);
	return true;
}

bool SWSSLSocket::use_DHfile(const char *dh_file, SWBaseError *error)
{
	if( !dh_file ){
		set_SSLError(error, fatal, "SWSSLSocket::use_DHfile() - Invalid argument");
		return false;
	}
	
	if( !check_ctx(error) )
		return false;
	
	// Set up DH stuff
	DH *dh;
	FILE *paramfile;

	paramfile = fopen(dh_file, "r");
	if( paramfile ){
		dh = PEM_read_DHparams(paramfile, NULL, NULL, NULL);
		fclose(paramfile);
		
		if( !dh ){
			handle_ERRerror(error, badFile, "SWSSLSocket::use_DHfile() on file" + string(dh_file) + ": ");
			return false;
		}
	}else{
		handle_errno(error, "SWSSLSocket::use_DHfile() on file " + string(dh_file) + ": ");
		return false;
	}
	
	SSL_CTX_set_tmp_dh(ctx, dh);
	
	DH_free(dh);
	
	no_error(error);
	return true;
}

bool SWSSLSocket::use_verification(const char *ca_file, const char *ca_dir, SWBaseError *error)
{
	if( !ca_file && !ca_dir ){
		// We must have atleast one set
		set_SSLError(error, fatal, "SWSSLSocket::use_verification() - Invalid argument");
		return false;
	}
	
	if( !check_ctx(error) )
		return false;

	if( ca_file ){
		if( !SSL_CTX_load_verify_locations(ctx, ca_file, NULL) ){
			handle_ERRerror(error, badFile, "SWSSLSocket::use_verification() on file " + string(ca_file) + " ");
			return false;
		}
	}
	
	if( ca_dir ){
		if( !SSL_CTX_load_verify_locations(ctx, NULL, ca_dir) ){
			handle_ERRerror(error, badFile, "SWSSLSocket::use_verification() on dir " + string(ca_dir) + " ");
			return false;
		}
	}

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE, verify_callback);
	
	no_error(error);
	return true;
}

SWSSLSocket::verify_info SWSSLSocket::get_verify_result(void)
{
	verify_info info;
	info.result = noCert;
	info.error = "";
	
	if( !ssl )
		return info;
		
	X509 *peer;
	long result;
	verify_result ret;
	
	if( (peer = SSL_get_peer_certificate(ssl)) ){
		result = SSL_get_verify_result(ssl);
		
		if( result == X509_V_OK )
			ret = CertOk;
		else if( result == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT )
			ret = noIssCert;
		else if( result == X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE )
			ret = CertError;
		else if( result == X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE )
			ret = CertError;
		else if( result == X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY )
			ret = CertError;
		else if( result == X509_V_ERR_CERT_SIGNATURE_FAILURE )
			ret = CertError;
		else if( result == X509_V_ERR_CERT_NOT_YET_VALID )
			ret = CertExpired;
		else if( result == X509_V_ERR_CERT_HAS_EXPIRED )
			ret = CertExpired;
		else if( result == X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD )
			ret = CertExpired;
		else if( result == X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD )
			ret = CertExpired;
		else if( result == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT )
			ret = CertSelfSign;
		else if( result == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN )
			ret = CertSelfSign;
		else if( result == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY )
			ret = noIssCert;
		else 
			ret = CertError;
		
		string msg = X509_verify_cert_error_string(result);
		info.result = ret;
		info.error = msg;
		
		X509_free(peer);
	}
	
	return info;
}

// why is NID_uniqueIdentifier undefined?
#ifndef NID_uniqueIdentifier
  #define NID_uniqueIdentifier 102
#endif
bool SWSSLSocket::get_peerCert_info(SWSSLSocket::peerCert_info *info, SWBaseError *error)
{
	if( !ssl || !info ){
		set_SSLError(error, fatal, "SWSSLSocket::get_peerCert_info() - Structures not allocated");
		return false;
	}
		
	X509 *peer;
	
	if( (peer = SSL_get_peer_certificate(ssl)) ){
		char buf[256];
		buf[255] = '\0';
		
		
		/*
		 * Get X509_NAME information
		*/
		
		X509_NAME *n = X509_get_issuer_name(peer);
		if( !n ){
			X509_free(peer);
			set_SSLError(error, fatal, "SWSSLSocket::get_peerCert_info() - Couldn't get issuer");
			return false;
		}
		
		if( X509_NAME_get_text_by_NID(n, NID_commonName, buf, 256) > 0 )
			info->commonName = buf;
		else
			info->commonName = "";
			
		if( X509_NAME_get_text_by_NID(n, NID_countryName, buf, 256) > 0 )
			info->countryName = buf;
		else
			info->countryName = "";	
			
		if( X509_NAME_get_text_by_NID(n, NID_localityName, buf, 256) > 0 )
			info->localityName = buf;
		else
			info->localityName = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_stateOrProvinceName, buf, 256) > 0 )
			info->stateOrProvinceName = buf;
		else
			info->stateOrProvinceName = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_organizationName, buf, 256) > 0 )
			info->organizationName = buf;
		else
			info->organizationName = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_organizationalUnitName, buf, 256) > 0 )
			info->organizationalUnitName = buf;
		else
			info->organizationalUnitName = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_title, buf, 256) > 0 )
			info->title = buf;
		else
			info->title = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_initials, buf, 256) > 0 )
			info->initials = buf;
		else
			info->initials = "";
			
		if( X509_NAME_get_text_by_NID(n, NID_givenName, buf, 256) > 0 )
			info->givenName = buf;
		else
			info->givenName = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_surname, buf, 256) > 0 )
			info->surname = buf;
		else
			info->surname = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_description, buf, 256) > 0 )
			info->description = buf;
		else
			info->description = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_uniqueIdentifier, buf, 256) > 0 )
			info->uniqueIdentifier = buf;
		else
			info->uniqueIdentifier = "";
		
		if( X509_NAME_get_text_by_NID(n, NID_pkcs9_emailAddress, buf, 256) > 0 )
			info->emailAddress = buf;
		else
			info->emailAddress = "";
		
		/*
		 * Get expire dates
		 * It seems impossible to get the time in time_t format
		*/
		
		info->notBefore = "";
		info->notAfter = "";
				
		BIO *bio = BIO_new(BIO_s_mem());
		int len;
		
		if( bio ){
			if( ASN1_TIME_print(bio, X509_get_notBefore(peer)) )
				if( (len = BIO_read(bio, buf, 255)) > 0 ){
					buf[len] = '\0';
					info->notBefore = buf;
				}
				
			if( ASN1_TIME_print(bio, X509_get_notAfter(peer)) )
				if( (len = BIO_read(bio, buf, 255)) > 0 ){
					buf[len] = '\0';
					info->notAfter = buf;
				}
						
			BIO_free(bio);
		}
		
		
		/*
		 * Misc. information
		*/
		info->serialNumber = ASN1_INTEGER_get(X509_get_serialNumber(peer));
		info->version = X509_get_version(peer);
		
		// Signature algorithm
		int nid = OBJ_obj2nid(peer->sig_alg->algorithm);
		if( nid != NID_undef )
			info->sgnAlgorithm = OBJ_nid2sn(nid);
		else
			info->sgnAlgorithm = "";	

		// Key algorithm
		EVP_PKEY *pkey = X509_get_pubkey(peer);
		if( pkey ){
			info->keyAlgorithm = OBJ_nid2sn(pkey->type);
			info->keySize = 8 * EVP_PKEY_size(pkey);
		}else{
			info->keyAlgorithm = "";
			info->keySize = -1;
		}
		
		X509_free(peer);
		
		no_error(error);
		return true;
	}

	set_SSLError(error, fatal, "SWSSLSocket::get_peerCert_info() - No peer certificate");
	return false;
}

// Copy the cert to string "pem" in PEM (ASCII) format
int SWSSLSocket::get_cert_PEM(X509 *cert, string *pem, SWBaseError *error)
{
	if( !cert || !pem ){
		set_SSLError(error, fatal, "SWSSLSocket::get_cert_PEM() - Structures not allocated");
		return -1;
	}
				
	char *buf;
	int len = -1;
	BIO *bio = BIO_new(BIO_s_mem());
		
	if( bio ){
		PEM_write_bio_X509(bio, cert);
		len = BIO_pending(bio);
			
		if( len > 0 ){
			buf = new char[len+1];
			len = BIO_read(bio, buf, len);
			buf[len] = '\0';
			*pem = buf;
			delete[] buf;
			
			no_error(error);
			return len;
		}
		BIO_free(bio);
	}

	set_SSLError(error, fatal, "SWSSLSocket::get_cert_PEM() - Couldn't create memory BIO");
	return -1;
}

int SWSSLSocket::get_peerCert_PEM(string *pem, SWBaseError *error)
{
	X509 *peer = SSL_get_peer_certificate(ssl);

	if( !peer ){
		set_SSLError(error, fatal, "SWSSLSocket::get_peerCert_PEM() - No peer certificate");
		return -1;
	}
	
	int ret = -1;
	SWSSLError err;
	ret = get_cert_PEM(peer, pem, &err);
	
	X509_free(peer);
	
	if( ret < 0 ){
		set_SSLError(error, err, err.get_error());
		return -1;
	}
	
	no_error(error);
	return ret;
}

bool SWSSLSocket::check_ctx(SWBaseError *error){
	if( !ctx ){
		//init new generic CTX object
		ctx = SSL_CTX_new(SSLv23_method());
		
		if( !ctx ){
			handle_ERRerror(error, fatal, "SWSSLSocket::create_ctx() ");
			return false;
		}
		
		//SSL_CTX_set_options(ctx, SSL_OP_ALL);
		SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
	}
	
	return true;
}

// Create temp cert if needed
bool SWSSLSocket::check_cert(SWBaseError *error)
{
	// FIXME: rsa_key, evp_pkey and cert are never deleted.
	// However, they are only created once so there is no
	// memory leak.
	
	if( have_cert )
		return true;  // No need to create a new temp cert
	
	static bool created_session_data = false;
	static RSA *rsa_key = NULL;
	static EVP_PKEY *evp_pkey = NULL;
	static X509 *cert = NULL;
	
	// Create a session certificate (gloabal for all instances of this class)
	// if no other certificate was provided
	if( !created_session_data ){	
		if( !rsa_key ){
			if( !(rsa_key = generate_rsakey(rsa_keysize)) ){
				handle_ERRerror(error, fatal, "SWSSLSocket::handle_crypto_data() ");
				return false;	
			}
		}
		
		if( !evp_pkey ){
			if( !(evp_pkey = generate_pkey(rsa_key)) ){
				handle_ERRerror(error, fatal, "SWSSLSocket::handle_crypto_data() ");
				return false;
			}
		}
		
		if( !(cert = BuildCertificate("SocketW session cert", NULL, NULL, evp_pkey)) ){
			handle_ERRerror(error, fatal, "SWSSLSocket::handle_crypto_data() ");
			return false;
		}
		
		created_session_data = true;
	}
	
	// Use our session certificate
	SSL_CTX_use_RSAPrivateKey(ctx, rsa_key);
	SSL_CTX_use_certificate(ctx, cert);

	have_cert = true;
	
	no_error(error);
	return true;
}

SWBaseSocket* SWSSLSocket::create(int socketdescriptor, SWBaseError *error)
{
	SWSSLSocket* remoteClass;
		
	/* Create new class*/
	remoteClass = new SWSSLSocket(block_mode, rsa_keysize);
	remoteClass->myfd  = socketdescriptor;
	
	// Copy CTX object pointer
	remoteClass->ctx  = ctx;
	if( ctx )
		ctx->references++;  // We don't want our destructor to delete ctx if still in use
	
	remoteClass->have_cert = have_cert; // Do CTX have cert loaded?
	
	SWSSLError err;
	
	/* Init SSL connection  (server side) */
	if( !remoteClass->ssl_accept(&err) ){
		delete remoteClass;
		set_SSLError(error, err, err.get_error());
		return NULL;
	}
	
	no_error(error);
	return remoteClass;
}

bool SWSSLSocket::ssl_accept(SWBaseError *error)
{	
	if( !check_ctx(error) )
		return false;
	
	if( ssl ){
		// Shouldn't be possible...
		SSL_free(ssl);
		ssl = NULL;
	}
	
	if( !check_cert(error) )
		return false;
	
	ssl = SSL_new(ctx);
	if( !ssl ){
		handle_ERRerror(error, fatal, "SWSSLSocket::ssl_accept() ");
		return false;
	}
	
	//init SSL server session
	SSL_set_accept_state(ssl);
	
	//get SSL to use our socket
	if( SSL_set_fd(ssl, myfd) < 1 ){
		handle_ERRerror(error, fatal, "SWSSLSocket::ssl_accept() ");
		return false;
	}
	
	int tmp;
	
	//get SSL to handshake with client
	if( (tmp = SSL_accept(ssl)) < 1 ){
		handle_SSLerror(error, tmp, "SWSSLSocket::ssl_accept() ");
		return false;
	}
	
	no_error(error);
	return true;
}

bool SWSSLSocket::connect(int port, string hostname, SWBaseError *error)
{
	if( !check_ctx(error) )
		return false;
	
	if( ssl == NULL )
		ssl = SSL_new(ctx);
	else
		SSL_clear(ssl);  //reuse old
		
	if( !ssl ){
		handle_ERRerror(error, fatal, "SWSSLSocket::connect() ");
		return false;
	}
	
	//init SSL client session
	SSL_set_connect_state(ssl);
	
	//normal connect
	if( !SWInetSocket::connect(port, hostname, error))
		return false;
	
	//get SSL to use our socket
	if( SSL_set_fd(ssl, myfd) < 1 ){
		handle_ERRerror(error, fatal, "SWSSLSocket::connect() ");
		return false;
	}
	
	int tmp;
	
	//get SSL to handshake with server
	if( (tmp = SSL_connect(ssl)) < 1 ){
		handle_SSLerror(error, tmp, "SWSSLSocket::connect() ");
		return false;
	}
	
	no_error(error);
	return true;
}

bool SWSSLSocket::disconnect(SWBaseError *error)
{
	if( ssl == NULL){
		set_error(error, notConnected, "SWSSLSocket::disconnect() - No connection");
		return false;
	}

 	//Send SSL shutdown signal to peer
 	if( SSL_shutdown(ssl) == -1 ){ 
		handle_SSLerror(error, -1, "SWSSLSocket::disconnect() " );
 		return false;
 	}

	//reset state
	reset();

	close(myfd);
	myfd = -1;

	no_error(error);
	return true;
}

int SWSSLSocket::send(const char *buf, int bytes, SWBaseError *error)
{
	int ret;

	if( ssl == NULL ){
		set_error(error, fatal, "SWSSLSocket::send() - No SSL session");
		return -1;
	}
	
	if ( (ret=SSL_write(ssl, buf, bytes)) < 1 ){
		if( ret == 0 ){
			recv_close = true;  //we  recived a close signal from peer
			set_error(error, terminated, "SWSSLSocket::send() - SSL connection terminated by peer");	
		}else
			handle_SSLerror(error, ret, "SWSSLSocket::send() ");	
		
		return -1;
	}
	
	no_error(error);
	return ret;
}

int SWSSLSocket::recv(char *buf, int bytes, SWBaseError *error)
{
	int ret;

	if( ssl == NULL ){
		set_error(error, fatal, "SWSSLSocket::recv() - No SSL session");
		return -1;
	}
	
	if( !waitRead(error) )
		return -1;
	else
		ret=SSL_read(ssl, buf, bytes);
		
 	if( ret < 1 ){
		if( ret == 0 ){
			recv_close = true;  //we  recived a close signal from peer
			set_error(error, terminated, "SWSSLSocket::recv() - SSL connection terminated by peer");
		}else
			handle_SSLerror(error, ret, "SWSSLSocket::recv() ");		
		
		return -1;
	}

	no_error(error);
	return ret;
}

extern bool sw_DoThrow, sw_Verbose; // Defined in sw_base.cxx
void SWSSLSocket::set_SSLError(SWBaseError *error, SWSSLError name, string msg)
{
	// Can this be handled by the standard error handling?
	if( name.se == noSSLError ){
		// ok, not a SSL specific error type
		set_error(error, name.be, msg);
		return;
	}
	
	error_string = msg;

	if(error != NULL){
		SWSSLError *err;
		
		if( (err = dynamic_cast<SWSSLError*>(error)) ){
			// Ok, "error" is a SWSSLError class, we can set the SSL specific error
			err->be = name.be;
			err->se = name.se;
			err->error_string = msg;
			err->failed_class = this;
		} else {
			// Standard error handling (must be handled by SWBaseSocket::set_error())
			set_error(error, name.be, msg);
		}
	}else{
		if( sw_Verbose )
			print_error();
		
		if( sw_DoThrow ){
			SWSSLError e;
			e.be = name.be;
			e.se = name.se;
			e.error_string = msg;
			e.failed_class = this;
			throw e;
		}else
			exit(-1);	
	}
}

void SWSSLSocket::handle_ERRerror(SWBaseError *error, SWSSLError name, string msg)
{
	msg += ERR_error_string(ERR_get_error(), NULL);
	
	set_SSLError(error, name, msg);
}

void SWSSLSocket::handle_SSLerror(SWBaseError *error, int ret, string msg)
{
	int val = SSL_get_error(ssl, ret);
	
	if( val == SSL_ERROR_SYSCALL ){
		int tmp = ERR_peek_error();  //don't remove from stack
		if( tmp != 0 ){
			//ok, we have an ERRlib error
			handle_ERRerror(error, fatal, msg);
			return;
		}else if( ret == -1 ){
			//normal system error
			handle_errno(error, msg);
			return;
		}
	}else if( val == SSL_ERROR_SSL ){
		//ok, we have an ERRlib error
		handle_ERRerror(error, fatal, msg);
		return;
	}
	

	SWBaseError err;
	
	if( val == SSL_ERROR_WANT_READ || val == SSL_ERROR_WANT_WRITE ){
		err = notReady;
		msg += "error: SSL Read/Write operation didn't complete, the same function should be called again later";
	}else if( val == SSL_ERROR_WANT_CONNECT /*|| val ==  SSL_ERROR_WANT_ACCEPT*/ ){
		err = notReady;
		msg += "error: SSL Connect/Accept operation didn't complete, the same function should be called again later";
	}else if( val == SSL_ERROR_WANT_X509_LOOKUP ){
		err = notReady;
		msg += "error: SSL operation didn't complete, the same function should be called again later";
	}else if( val == SSL_ERROR_ZERO_RETURN){
		//Not used... we don't care if it was done cleanly or not for now
		recv_close = true;
		err = terminated;
		msg += "error: SSL connection closed by peer";
	}else if( val == SSL_ERROR_SYSCALL ){
		//EOF error
		err = fatal;
		msg += "error: SSL EOF observed that violates the protocol";
	}else{
		//default
		err = fatal;
		msg += "error: Unknown SSL error";
	}
	
	set_error(error, err, msg);	
}

#endif /* _HAVE_SSL */

