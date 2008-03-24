// C++ Socket Wrapper
// SocketW SSL socket header
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

#ifndef sw_ssl_H
#define sw_ssl_H

#include "sw_internal.h"

#ifdef _HAVE_SSL

#include "sw_inet.h"
#include <string>

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

//Default values for RSA key generation
#define RSA_KEYSIZE 512
#define RSA_KEYEXP RSA_F4 /* 65537 */

// Simple streaming SSL TCP/IP class
class DECLSPEC SWSSLSocket : public SWInetSocket
{
public:
	SWSSLSocket(block_type block=blocking, int keysize=RSA_KEYSIZE);
	virtual ~SWSSLSocket();
	
	// SWSSLSocket specific error types
	// noSSLError   - internal, DO NOT USE
	// badPasswd    - wrong password for private key
	// badFile      - Couldn't access or use file
	enum ssl_error{noSSLError, badPasswd, badFile};
	
	class DECLSPEC SWSSLError : public SWBaseError
	{
	public:
		SWSSLError();
		SWSSLError(ssl_error e);
		SWSSLError(base_error e);
		
		virtual bool operator==(SWSSLError e);
		virtual bool operator!=(SWSSLError e);
	
	protected:
		friend class SWSSLSocket;
		
		// The SSL error type
		ssl_error se;
	};
	
	virtual bool connect(int port, std::string hostname, SWBaseError *error = NULL);
	virtual bool disconnect(SWBaseError *error = NULL);
	
	virtual int send(const char *buf, int bytes, SWBaseError *error = NULL);
	virtual int recv(char *buf, int bytes=256, SWBaseError *error = NULL);
	
	// Cert files (if not set, a temporary RSA session cert will be created if needed)
	// PEM format
	bool use_cert(const char *cert_file, const char *private_key_file, SWBaseError *error = NULL);
	
	// As use_cert() but also give password for private_key_file 
	// (or get OpenSSL's standard prompt each time)
	bool use_cert_passwd(const char *cert_file, const char *private_key_file, std::string passwd, SWBaseError *error = NULL);
	
	// Or specify a password callback given to OpenSSL that hands back
	// the password to be used during decryption. On invocation a pointer
    // to userdata is provided. The pem_passwd_cb must write the password into
    // the provided buffer buf which is of size size. The actual length of the
    // password must be returned to the calling function. rwflag indicates
    // whether the callback is used for reading/decryption (rwflag=0) or 
	// writing/encryption (rwflag=1).
	// See man SSL_CTX_set_default_passwd_cb(3) for more information.
	bool use_cert_cb(const char *cert_file, const char *private_key_file, int passwd_cb(char *buf, int size, int rwflag, void *userdata), char *userdata = NULL, SWBaseError *error = NULL);
	
	// Use Diffie-Hellman key exchange?
	// See man SSL_CTX_set_tmp_dh_callback(3) for more information.
	bool use_DHfile(const char *dh_file, SWBaseError *error = NULL);
	
	// Should the peer certificate be verified?
	// The arguments specifies the locations at which CA
	// certificates for verification purposes are located. The
	// certificates available via ca_file and ca_dir are trusted.
	// See man SSL_CTX_load_verify_locations(3) for format information.
	bool use_verification(const char *ca_file, const char *ca_dir, SWBaseError *error = NULL);
	
	// Get peer certificate verification result
	// Should be called after connect() or accept() where the verification is done.
	// On the server side (i.e accept()) this should be done on the new class returned
	// by accept() and NOT on the listener class!
	enum verify_result{noCert, CertOk, noIssCert, CertExpired, CertSelfSign, CertError};
	struct verify_info{
		verify_result result;
		std::string error;
	};
	verify_info get_verify_result(void);
	
	// Get information about peer certificate
	// Should be called after connect() or accept() when using verification
	struct peerCert_info{
		// Issuer name
		std::string commonName;             // CN
		std::string countryName;            // C
		std::string localityName;           // L
		std::string stateOrProvinceName;    // ST
		std::string organizationName;       // O
		std::string organizationalUnitName; // OU
		std::string title;                  // T
		std::string initials;               // I
		std::string givenName;              // G
		std::string surname;                // S
		std::string description;            // D
		std::string uniqueIdentifier;       // UID
		std::string emailAddress;           // Email
		
		// Expire dates
		std::string notBefore;
		std::string notAfter;
		
		// Misc. data
		long serialNumber;
		long version;
		std::string sgnAlgorithm;
		std::string keyAlgorithm;
		int keySize;
	};
	bool get_peerCert_info(peerCert_info *info, SWBaseError *error = NULL);
	
	// Get peer certificate in PEM (ASCII) format
	// Should be called after connect() or accept() when using verification
	// Returns the length of pem or -1 on errors
	int get_peerCert_PEM(std::string *pem, SWBaseError *error = NULL);

protected:
	// The SWSSLSocket version if create()
	// Called from SWBaseSocket::accept()
	virtual SWBaseSocket* create(int socketdescriptor, SWBaseError *error);
	
	// Init server side SSL connection. Used in create()
	virtual bool ssl_accept(SWBaseError *error); 
	
	// Create new CTX if none is available
	virtual bool check_ctx(SWBaseError *error);
	
	// Create temp cert if no other is loaded
	virtual bool check_cert(SWBaseError *error);
	
	// Helper function: converts from X509 format to ASCII PEM format
	int get_cert_PEM(X509 *cert, std::string *pem, SWBaseError *error);
	
	// Internal error handling
	virtual void set_SSLError(SWBaseError *error, SWSSLError name, std::string msg);
	virtual void handle_ERRerror(SWBaseError *error, SWSSLError name, std::string msg);
	virtual void handle_SSLerror(SWBaseError *error, int ret, std::string msg);
	
	// SSL data
	SSL_CTX *ctx;
	SSL *ssl;
	
	bool have_cert;  // Indicate CERT loaded or created
	int rsa_keysize; // keysize argument from constructor
	char *ud;        // Userdata
};

#endif /* _HAVE_SSL */
#endif /* sw_ssl_H */
