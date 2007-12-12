// SSL telnet
// A bit more advanced example than the simple server & client examples.
// Makes a "telnet" connection to an SSL server. Uses fork() to handle two
// dataflows; select() should really be used instead if you need to handle 
// more complex situations.

#include "SocketW.h"
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

// Our signal handler
bool interrupted = false;
void sighandler(int signum)
{
	interrupted = true;
	signal(SIGINT, SIG_DFL); // Activate ctrl+c again if we screwup
}


int main(void)
{
	string host = "localhost";
	int port    = 5555;

	SWSSLSocket mySocket;
	SWBaseSocket::SWBaseError error;
	
	//Try to connect to socket
	printf("Connecting to %s:%d", host.c_str(), port);fflush(NULL);
	int i = 0;
	while(true){
		mySocket.connect(port, host, &error);
		
		// check for errors
		if( error == SWBaseSocket::ok )
			break;  // ok, connected
		else if( error != SWBaseSocket::noResponse ){
			// something went wrong
			mySocket.print_error();
			exit(-1);
		}
		
		//No response, try again later
		if( i > 20 ){
			printf("\nClient timeout!\n");
			exit(-1);
		}
		
		printf("."); fflush(NULL);
		sleep(3);
		i++;
	}
	printf("\nConnected to %s:%d (%s)\n\n", mySocket.get_peerName().c_str(), mySocket.get_peerPort(), mySocket.get_peerAddr().c_str());
	
	// Create a new process for handling input from stdin and then handle stdout ourself
	
	pid_t pid;
	if( (pid = fork()) == 0 ){
		//send process
	
		char buf[256];
		int size;
		buf[255] = '\0';
		
		while( true ){
			fgets(buf, 253, stdin); // read one line from stdin
			
			size = strlen(buf);
			if(size > 253)
				size=253;
			
			// Add \r\n (CRLF - standard telnet end of line)
			buf[size] = '\r';
			buf[size+1] = '\n';
			
			mySocket.send(buf, size+2); // send to server
		}
	
		return 0;
	}
	
	// recive process
		
	int size = 0;
	char buf[257];
	buf[256] = '\0';
	
	// Handle ctrl+c
	signal(SIGINT, sighandler);  // our sighandler() function will be called on ctrl+c
		
	while( true ){
		size = mySocket.recv(buf, 256, &error);
		
		// handle errors and interruptions
		if( error != SWBaseSocket::ok || interrupted == true ){
			
			kill(pid, SIGKILL); // Kill send process
			wait(NULL);         // Wait for send process to die
				
			if( error == SWBaseSocket::terminated || interrupted == true ){
				printf("\nConnection to %s closed.\n", host.c_str());
				mySocket.disconnect();
				
				return 0;
			}
			
			// something went wrong
			mySocket.print_error();
			mySocket.disconnect();
			return -1;
		}
				
		write(1, buf, size);  // write to stdout
	}
	
	return 0;
}
