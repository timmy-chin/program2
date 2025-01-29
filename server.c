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
#include "handleTable.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

int recvFromClient(int clientSocket, uint8_t * dataBuffer);
int checkArgs(int argc, char *argv[]);
int addNewSocket(int mainServerSocket);
void processClient(int clientSocket, HandleTable * table);
void serverControl(int mainServerClient, HandleTable * table);
void sendToClient(int clientSocket, uint8_t * sendBuf, int sendLen);
void processFlag(int flag, uint8_t * dataBuffer, int clientSocket, HandleTable * table, int messageLen);
void registerHandle(uint8_t * dataBuffer, int clientSocket, HandleTable * table);
void uniMultiCastHandle(uint8_t * dataBuffer, int clientSocket, HandleTable * table, int messageLen);
void getMessage(uint8_t * dataBuffer, int index, char * buffer);
void getRecipients(uint8_t * dataBuffer, char ** recipients, uint8_t * handle_count);
void batchSend(char ** recipients, uint8_t handle_count, uint8_t * data_buffer, HandleTable * table, int messageLen, int senderSocket);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int clientSocket = 0;   //socket descriptor for the client socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	HandleTable table = newHandleTable();

	serverControl(mainServerSocket, &table);
	
	/* close the sockets */
	close(clientSocket);
	close(mainServerSocket);

	
	return 0;
}

int recvFromClient(int clientSocket, uint8_t * dataBuffer)
{
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		return messageLen;
	}
	else
	{
		printf("Connection closed by other side\n");
		removeFromPollSet(clientSocket);
		close(clientSocket);
		return -1;
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

void serverControl(int mainServerSocket, HandleTable * table) {
	setupPollSet();
	addToPollSet(mainServerSocket);
	while (1) {
		int activeSocket = pollCall(-1); // poll until one is ready
		if (activeSocket == mainServerSocket) {
			int clientSocket = addNewSocket(mainServerSocket);
			addToPollSet(clientSocket);
		}
		else {
			processClient(activeSocket, table);
		}
	}
}

int addNewSocket(int mainServerSocket) {
	// wait for client to connect
	int clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);
	return clientSocket;
}

void processClient(int clientSocket, HandleTable * table){
	uint8_t dataBuffer[MAXBUF];
	int messageLen = recvFromClient(clientSocket, dataBuffer);
	if (messageLen > 0){
		int flag = dataBuffer[0];
		processFlag(flag, dataBuffer, clientSocket, table, messageLen);
	} 
}

void sendToClient(int clientSocket, uint8_t * sendBuf, int sendLen) {
	int sent =  sendPDU(clientSocket, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
}

void processFlag(int flag, uint8_t * dataBuffer, int clientSocket, HandleTable * table, int messageLen) {
	if (flag == 1) { // register new handle
		registerHandle(dataBuffer, clientSocket, table);
	}
	else if (flag == 5 || flag == 6) {  // unicast and multicast
		uniMultiCastHandle(dataBuffer, clientSocket, table, messageLen);
	}
}

void registerHandle(uint8_t * dataBuffer, int clientSocket, HandleTable * table) {
	uint8_t handle_length = dataBuffer[1];
	char * handle_name = (char *)malloc(sizeof(char)*100);
	memcpy(handle_name, dataBuffer + 2, sizeof(char) * handle_length);
	if (getByHandle(table, handle_name) != -1) {  // handle already exist
		printf("Registration Rejected due to Handle %s already existing\n", handle_name);
		uint8_t errorFlag = 3;
		sendPDU(clientSocket, &errorFlag, 1);
	}
	else {
		printf("Registration succeeded for handle %s\n", handle_name);
		uint8_t successFlag = 2;
		sendPDU(clientSocket, &successFlag, 1);
		addToTable(table, handle_name, clientSocket);
	}
}

void uniMultiCastHandle(uint8_t * dataBuffer, int clientSocket, HandleTable * table, int messageLen){
	char * recipients[10];
	uint8_t handle_count;  // number of handles to send message
	getRecipients(dataBuffer, recipients, &handle_count);
	batchSend(recipients, handle_count, dataBuffer, table, messageLen, clientSocket);
}

void getMessage(uint8_t * dataBuffer, int index, char * buffer) {
	int i = 0;
	int j = index;
	while (dataBuffer[j] != '\0'){
		buffer[i] = dataBuffer[j];
		i++;
		j++;
	}
}

void getRecipients(uint8_t * dataBuffer, char ** recipients, uint8_t * handle_count) {
	int lastIndex = 1;

	// By pass sender information
	uint8_t sender_length;
	getFromPDU(dataBuffer, &sender_length, 1, &lastIndex);
	lastIndex = lastIndex + sender_length;

	getFromPDU(dataBuffer, handle_count, 1, &lastIndex);
	for (int i = 0; i < *handle_count; i++) {
		uint8_t handle_length;
		getFromPDU(dataBuffer, &handle_length, 1, &lastIndex);
		char * handle_name = (char *)malloc(sizeof(char)*100);
		getFromPDUWithChar(dataBuffer, handle_name, handle_length, &lastIndex);
		recipients[i] = handle_name;
	}
}

void batchSend(char ** recipients, uint8_t handle_count, uint8_t * data_buffer, HandleTable * table, int messageLen, int senderSocket) {
	for (int i = 0; i < handle_count; i++) {
		int recipient_socket = getByHandle(table, recipients[i]);
		if (recipient_socket != -1) {
			sendPDU(recipient_socket, data_buffer, messageLen);
		}
		else {
			// send error message flag 7
			uint8_t errorBuffer[MAXBUF];
			uint8_t flag = 7;
			int lastIndex = 0;
			uint8_t recipient_handle_length = strlen(recipients[i]) + 1;
			createPDU(errorBuffer, &flag, 1, &lastIndex);
			createPDU(errorBuffer, &recipient_handle_length, 1, &lastIndex);
			createPDUWithChar(errorBuffer, recipients[i], recipient_handle_length, &lastIndex);
			int data_length = lastIndex;  // the last Index of the data buffer is the length
			sendPDU(senderSocket, errorBuffer, data_length);
		}
		free(recipients[i]);
	}
}
