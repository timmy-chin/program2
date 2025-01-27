/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "pduUtil.h"
#include "pollLib.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
int addNewSocket(int mainServerSocket);
void processClient(int clientSocket);
void serverControl(int mainServerClient);
void sendToClient(int clientSocket, uint8_t * sendBuf, int sendLen);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int clientSocket = 0;   //socket descriptor for the client socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	serverControl(mainServerSocket);
	
	/* close the sockets */
	close(clientSocket);
	close(mainServerSocket);

	
	return 0;
}

void recvFromClient(int clientSocket)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		printf("Message received, length: %d Data: %s\n", messageLen, dataBuffer);
		sendToClient(clientSocket, dataBuffer, messageLen);
	}
	else
	{
		printf("Connection closed by other side\n");
		removeFromPollSet(clientSocket);
		close(clientSocket);
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

void serverControl(int mainServerSocket) {
	setupPollSet();
	addToPollSet(mainServerSocket);
	while (1) {
		int activeSocket = pollCall(-1); // poll until one is ready
		if (activeSocket == mainServerSocket) {
			int clientSocket = addNewSocket(mainServerSocket);
			addToPollSet(clientSocket);
		}
		else {
			processClient(activeSocket);
		}
	}
}

int addNewSocket(int mainServerSocket) {
	// wait for client to connect
	int clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);
	return clientSocket;
}

void processClient(int clientSocket){
	recvFromClient(clientSocket);
}

void sendToClient(int clientSocket, uint8_t * sendBuf, int sendLen) {
	int sent =  sendPDU(clientSocket, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
}

