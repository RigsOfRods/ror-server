// Client
// A simple SSL client.
// First start the server example and then run this client
// to test. Change "localhost" in connect() if you want to
// test over a real network.

#include "SocketW.h"
#include <stdio.h>
#include <unistd.h>

using namespace std;

int main(void)
{
	int i = 0;

	SWSSLSocket mySocket;
	SWSSLSocket::SWSSLError error;
	
	// Check server certificates agains our
	// known trusted certificate
	mySocket.use_verification("cert.pem", NULL);
	
	// Try to connect to server
	while(true){
		mySocket.connect(5555, "localhost", &error);  // connect to localhost port 5555
		
		// check for errors
		if( error == SWBaseSocket::ok )
			break;  // ok, connected
		else if( error != SWBaseSocket::noResponse ){
			// something went wrong
			mySocket.print_error();
			exit(-1);
		}
		
		// No response, try again later
		if( i > 20 ){
			printf("\nClient timeout!\n");
			exit(-1);
		}
		printf("."); fflush(NULL);
		sleep(3);
		i++;
	}
	printf("\n");
	
	// Get the result of the server certificate verification
	SWSSLSocket::verify_info vinfo;
	vinfo = mySocket.get_verify_result();
	printf("Server certificate verification: %s\n\n", vinfo.error.c_str());
	
	// Print server information
	SWSSLSocket::peerCert_info info;
	mySocket.get_peerCert_info(&info);
	printf("Server certificate information:\n");
	printf("Signature algorithm: %s\n", info.sgnAlgorithm.c_str());
	printf("Key algorithm: %s (%d bit)\n", info.keyAlgorithm.c_str(), info.keySize);
	printf("CN   : %s\n", info.commonName.c_str()); 
	printf("C    : %s\n", info.countryName.c_str());
	printf("L    : %s\n", info.localityName.c_str());
	printf("ST   : %s\n", info.stateOrProvinceName.c_str());
	printf("O    : %s\n", info.organizationName.c_str());
	printf("OU   : %s\n", info.organizationalUnitName.c_str());
	printf("T    : %s\n", info.title.c_str());
	printf("I    : %s\n", info.initials.c_str());
	printf("G    : %s\n", info.givenName.c_str());
	printf("S    : %s\n", info.surname.c_str());
	printf("D    : %s\n", info.description.c_str());
	printf("UID  : %s\n", info.uniqueIdentifier.c_str());
	printf("Email: %s\n", info.emailAddress.c_str());
	printf("Valid from %s to %s\n", info.notBefore.c_str(), info.notAfter.c_str());
	printf("Serialnumber: %ld, version: %ld\n", info.serialNumber, info.version);
	string pem;
	mySocket.get_peerCert_PEM(&pem);
	printf("\n%s\n\n", pem.c_str());
	
	// Send message to server
	printf("Client running on %s:%d (%s)\n", mySocket.get_hostName().c_str(), mySocket.get_hostPort(), mySocket.get_hostAddr().c_str());		
	printf("Sent %d bytes to %s:%d (%s)\n", mySocket.sendmsg("Hello Server!"), mySocket.get_peerName().c_str(), mySocket.get_peerPort(), mySocket.get_peerAddr().c_str());
	
	// Recive message from server
	string msg = mySocket.recvmsg(255);
	printf("Recived: %s\n", msg.c_str());
	
	mySocket.disconnect();
	
	return 0;
}
