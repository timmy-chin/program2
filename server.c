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

#define MAXBUF 1400
#define DEBUG_FLAG 1

int checkArgs(int argc, char *argv[]);
void serverControl(int mainServerClient, HandleTable *table);
int addNewSocket(int mainServerSocket);
void processClient(int clientSocket, HandleTable *table);
int recvFromClient(int clientSocket, uint8_t *dataBuffer);
void processFlag(int flag, uint8_t *dataBuffer, int clientSocket, HandleTable *table, int messageLen);
void registerHandle(uint8_t *dataBuffer, int clientSocket, HandleTable *table);
void handleUniMulticast(uint8_t *dataBuffer, int clientSocket, HandleTable *table, int messageLen);
void handleBroadcast(uint8_t *dataBuffer, HandleTable *table, int messageLen);
void handleList(int clientSocket, HandleTable *table);
void bulkSend(char **recipients, uint8_t handle_count, uint8_t *data_buffer, HandleTable *table, int messageLen, int senderSocket);
void broadcastSend(uint8_t *dataBuffer, HandleTable *table, int messageLen, char *senderHandle);
void sendListHandleCount(int clientSocket, HandleTable *table);
void sendListHandle(int clientSocket, char *handle);
void sendListDone(int clientSocket);
void getMessageFromPDU(uint8_t *dataBuffer, int index, char *buffer);
void getRecipientsFromPDU(uint8_t *dataBuffer, char **recipients, uint8_t *handle_count);

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
		else if (activeSocket < 0) {
			printf("Error in polling");
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

void processFlag(int flag, uint8_t * dataBuffer, int clientSocket, HandleTable * table, int messageLen) {
	if (flag == 1) { // register new handle
		registerHandle(dataBuffer, clientSocket, table);
	}
	else if (flag == 5 || flag == 6) {  // unicast and multicast
		handleUniMulticast(dataBuffer, clientSocket, table, messageLen);
	}
	else if (flag == 4) {  // broadcast
		handleBroadcast(dataBuffer, table, messageLen);
	}
	else if (flag == 10) {
		handleList(clientSocket, table);
	}
}

/* Add new handle to handle table with related socket */
void registerHandle(uint8_t * dataBuffer, int clientSocket, HandleTable * table) {
	uint8_t handle_length = dataBuffer[1];
	char * handle_name = (char *)malloc(sizeof(char)*100);
	memcpy(handle_name, dataBuffer + 2, sizeof(char) * handle_length);
	if (getByHandle(table, handle_name) != -1) {  // handle already exist
		uint8_t errorFlag = 3;
		sendPDU(clientSocket, &errorFlag, 1);
	}
	else {
		uint8_t successFlag = 2;
		sendPDU(clientSocket, &successFlag, 1);
		addToTable(table, handle_name, clientSocket);
	}
}

void handleUniMulticast(uint8_t * dataBuffer, int clientSocket, HandleTable * table, int messageLen){
	char * recipients[10];
	uint8_t handle_count;  // number of handles to send message
	getRecipientsFromPDU(dataBuffer, recipients, &handle_count);
	bulkSend(recipients, handle_count, dataBuffer, table, messageLen, clientSocket);
}

void handleBroadcast(uint8_t * dataBuffer, HandleTable * table, int messageLen) {
	int lastIndex = 1; // skip flag
	// Get sender information
	uint8_t sender_length;
	getFromPDU(dataBuffer, &sender_length, 1, &lastIndex);
	char senderHandle[100];
	getFromPDUWithChar(dataBuffer, senderHandle, sender_length, &lastIndex);
	broadcastSend(dataBuffer, table, messageLen, senderHandle);
}

void handleList(int clientSocket, HandleTable * table) {
	sendListHandleCount(clientSocket, table);
	for (int i = 0; i<table->size; i++) {
		char * handle = table->handleArray[i];
		sendListHandle(clientSocket, handle);
	}
	sendListDone(clientSocket);
}

/* Forward a PDU to a list of recipients */
void bulkSend(char ** recipients, uint8_t handle_count, uint8_t * data_buffer, HandleTable * table, int messageLen, int senderSocket) {
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
			uint8_t recipient_handle_length = strlen(recipients[i]);
			createPDU(errorBuffer, &flag, 1, &lastIndex);
			createPDU(errorBuffer, &recipient_handle_length, 1, &lastIndex);
			createPDUWithChar(errorBuffer, recipients[i], recipient_handle_length, &lastIndex);
			int data_length = lastIndex;  // the last Index of the data buffer is the length
			sendPDU(senderSocket, errorBuffer, data_length);
		}
		free(recipients[i]);
	}
}

/* Send to all except for the sender */
void broadcastSend(uint8_t * dataBuffer, HandleTable * table, int messageLen, char * senderHandle) {
	for (int i = 0; i < table->size; i++) {  // get all handles
		char * recipientHandle = table->handleArray[i];
		if (strcmp(senderHandle, recipientHandle) != 0) {  // not equal to the sender
			int recipientSocket = table->socketArray[i];
			sendPDU(recipientSocket, dataBuffer, messageLen);  // send to handle 
		}
	}
}

/* Sends the count of handle */
void sendListHandleCount(int clientSocket, HandleTable * table) {
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 11;
	int lastIndex = 0;
	uint8_t handle_count = table->size;
	createPDU(pduBuffer, &flag, 1, &lastIndex);
	createPDU(pduBuffer, &handle_count, 1, &lastIndex);
	int dataLength = lastIndex;
	sendPDU(clientSocket, pduBuffer, dataLength);
}

/* Sends all the handles */
void sendListHandle(int clientSocket, char * handle) {
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 12;
	int lastIndex = 0;
	uint8_t handle_len = strlen(handle);
	createPDU(pduBuffer, &flag, 1, &lastIndex); // flag 12
	createPDU(pduBuffer, &handle_len, 1, &lastIndex);  // handle len
	createPDUWithChar(pduBuffer, handle, handle_len, &lastIndex);  // handle name
	int dataLength = lastIndex;
	sendPDU(clientSocket, pduBuffer, dataLength);
}

/* Sends done signal for the list */
void sendListDone(int clientSocket){
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 13;
	int lastIndex = 0;
	createPDU(pduBuffer, &flag, 1, &lastIndex); // flag 13
	int dataLength = lastIndex;
	sendPDU(clientSocket, pduBuffer, dataLength);
}

/* Gets message from tail of PDU */
void getMessageFromPDU(uint8_t * dataBuffer, int index, char * buffer) {
	int i = 0;
	int j = index;
	while (dataBuffer[j] != '\0'){
		buffer[i] = dataBuffer[j];
		i++;
		j++;
	}
}

/* Gets all the recipients in a PDU for Uni and Multicast */
void getRecipientsFromPDU(uint8_t * dataBuffer, char ** recipients, uint8_t * handle_count) {
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
