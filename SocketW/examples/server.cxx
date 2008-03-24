// Server
// A simple SSL server.
// First start this server and then run the client example
// to test.

#include "SocketW.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

int main(void)
{
	SWSSLSocket listener;
	SWSSLSocket *mySocket;
	
	// Load our certificate
	listener.use_cert_passwd("cert.pem", "cert_key.pem", "qwerty");
	listener.use_DHfile("dh1024.pem");

	listener.bind(5555);  // Bind listener to port 5555
	listener.listen();
	
	// Print some info
	printf("Server running on %s:%d (%s)\n", listener.get_hostName().c_str(), listener.get_hostPort(), listener.get_hostAddr().c_str());
	
	while(true){
		mySocket = (SWSSLSocket *)listener.accept();  // wait for connections
		
		// Create a process for the new connection
		if( fork()==0 ){	
			listener.close_fd();  // we don't need it in this process
		
			// Recive message
			string msg = mySocket->recvmsg(255);
			
			printf("Recived: %s\n", msg.c_str());
			printf("%d bytes from %s:%d (%s)\n", msg.size(), mySocket->get_peerName().c_str(), mySocket->get_peerPort(), mySocket->get_peerAddr().c_str());

			// Send message
			mySocket->sendmsg("Hello Client!");

			mySocket->disconnect();
			delete mySocket;
			return 0;
		}
		
		delete mySocket;
	}
	
	return 0;
}
