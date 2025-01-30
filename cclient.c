/******************************************************************************
* myClient.c
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

#define MAXBUF 1400
#define DEBUG_FLAG 1

void checkArgs(int argc, char *argv[]);
void registerHandle(char *handle, int serverSocket);
void clientControl(int serverSocket, char *myHandle);
void processStdin(int serverSocket, char *myHandle, int *listMode);
void processMsgFromServer(int serverSocket, int *listMode);
int readFromStdin(uint8_t *buffer);
void sendUnicast(int serverSocket, uint8_t *userInput, char *myHandle);
void sendMulticast(int serverSocket, uint8_t *userInput, char *myHandle);
void sendBroadcast(int serverSocket, uint8_t *userInput, char *myHandle);
void sendList(int serverSocket, uint8_t *userInput, char *myHandle);
void receiveFlag(int flag, uint8_t *dataBuffer, int *listMode);
void receiveUniMultiCast(uint8_t *dataBuffer);
void receiveBroadcast(uint8_t *dataBuffer);
void receiveListHandleCount(uint8_t *dataBuffer);
void receiveListHandleName(uint8_t *dataBuffer);
void receiveHandleNotFound(uint8_t *dataBuffer);
void getUserInputBlock(uint8_t *string, int index, char *buffer);
void getUserInputMessage(uint8_t *string, int index, char *buffer);
void batchSend(int serverSocket, uint8_t *data_buffer, int message_length, char *message, int lastIndex);
void getSender(uint8_t *dataBuffer, char *senderBuffer, int *lastIndex);
void copyMessage(uint8_t *dataBuffer, int lastIndex, char *message);
void getMessageFromPDU(uint8_t *dataBuffer, char *message, int *lastIndex);


int main(int argc, char * argv[])
{
	int socketNum = 0;  //socket descriptor
	checkArgs(argc, argv);
	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	char * myHandle = argv[1];
	registerHandle(myHandle, socketNum);
	clientControl(socketNum, myHandle);
	close(socketNum);
	return 0;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
}

/* Sends new handle to server to be added to handle table */
void registerHandle(char * handle, int serverSocket){
	uint8_t handle_length = strlen(handle);
	if (handle_length > 100) {
		printf("Handle Length cannot be longer than 100 characters\n");
		exit(0);
	}
	uint8_t flag = 1;
	uint8_t data_buffer[MAXBUF];
	int lastIndex = 0;
	createPDU(data_buffer, &flag, 1, &lastIndex);
	createPDU(data_buffer, &handle_length, 1, &lastIndex);
	createPDUWithChar(data_buffer, handle, handle_length, &lastIndex);
	int data_length = lastIndex;
	sendPDU(serverSocket, data_buffer, data_length);
	uint8_t responseBuffer[MAXBUF];
	int messageLen = 0;
	//check responce from server for registration
	messageLen = recvPDU(serverSocket, responseBuffer, MAXBUF);
	if (messageLen > 0)
	{
		if (responseBuffer[0] == 2) {  // flag 2
			return; // Registration succeeded
		} 
		else if (responseBuffer[0] == 3) {  // flag 3
			printf("Handle %s registration failed because it already exist\n", handle);
			exit(0);
		}
		else {
			printf("Response flag %d unrecognized\n", responseBuffer[0]);
			exit(0);
		}
	}
	else
	{
		printf("\nServer has terminated\n");
		exit(0);
	}
}

void clientControl(int serverSocket, char *myHandle){
	int listMode = 0;  // skip $: when listing all handles
	setupPollSet();
	addToPollSet(STDIN_FILENO);
	addToPollSet(serverSocket);
	while (1) {
		if (listMode == 0) {
			printf("$:");
			fflush(stdout);  // force $: out to stdout
		}
		int activeSocket = pollCall(-1);
		if (activeSocket == STDIN_FILENO) {
			processStdin(serverSocket, myHandle, &listMode);
		}
		else if (activeSocket < 0) {
			printf("Error in polling\n");
		}
		else {
			processMsgFromServer(serverSocket, &listMode);
		}
	}
}

/* Process user input to be sent to server */
void processStdin(int serverSocket, char * myHandle, int * listMode){
	uint8_t userInput[MAXBUF]; 	
	readFromStdin(userInput);
	if (userInput[0] == '%') {
		if (userInput[1] == 'M' || userInput[1] == 'm') {  // unicast feature
			sendUnicast(serverSocket, userInput, myHandle);
		}
		else if (userInput[1] == 'C' || userInput[1] == 'c') {  // multicast feature
			sendMulticast(serverSocket, userInput, myHandle);
		}
		else if (userInput[1] == 'B' || userInput[1] == 'b') {  // broadcast
			sendBroadcast(serverSocket, userInput, myHandle);
		}
		else if (userInput[1] == 'L' || userInput[1] == 'l') {  // list all handles
			*listMode = 1;
			sendList(serverSocket, userInput, myHandle);
		}
		else {
			printf("\n%c is an invalid command, only M, C, B and L are accepted\n", userInput[1]);
		}
	}
	else {
		printf("Invalid Input, must start with percentage\n");
	}
}

/* Process message received from the server */
void processMsgFromServer(int serverSocket, int * listMode) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	//now get the data from the client_socket
	messageLen = recvPDU(serverSocket, dataBuffer, MAXBUF);
	if (messageLen > 0)
	{
		int flag = dataBuffer[0];
		receiveFlag(flag, dataBuffer, listMode);
	}
	else
	{
		printf("\nServer has terminated\n");
		exit(0);
	}
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

/* Send a unicast message to the server */
void sendUnicast(int serverSocket, uint8_t * userInput, char * myHandle) {
	char handleName[100];
	char message[MAXBUF];
	getUserInputBlock(userInput, 1, handleName);
	getUserInputMessage(userInput, 2, message);
	uint8_t handle_length = strlen(handleName);
	int message_length = strlen(message);
	uint8_t myHandle_length = strlen(myHandle);
	uint8_t flag = 5;
	uint8_t data_buffer[MAXBUF];
	uint8_t numOfHandles = 1;
	int lastIndex = 0;
	createPDU(data_buffer, &flag, 1, &lastIndex);  // flag
	createPDU(data_buffer, &myHandle_length, 1, &lastIndex);  // sender length
	createPDUWithChar(data_buffer, myHandle, myHandle_length, &lastIndex);  // sender handle
	createPDU(data_buffer, &numOfHandles, 1, &lastIndex);  // specify 1 handle 
	createPDU(data_buffer, &handle_length, 1, &lastIndex);  // dest handle length
	createPDUWithChar(data_buffer, handleName, handle_length, &lastIndex);  // dest handle
	batchSend(serverSocket, data_buffer, message_length, message, lastIndex);  // send in batch of 200 char messages
}

void sendMulticast(int serverSocket, uint8_t * userInput, char * myHandle) {
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 6;
	int lastIndex = 0;
	uint8_t sender_len = strlen(myHandle);
	char handle_count_str[3];
	getUserInputBlock(userInput, 1, handle_count_str);
	uint8_t handle_count = atoi(handle_count_str);
	if (handle_count > 9) {
		printf("Handle count cannot be larger than 9\n");
		return;
	}
	if (handle_count == 1) {
		printf("Use Unicast for 1 handle\n");
		return;
	}
	createPDU(pduBuffer, &flag, 1, &lastIndex);  // flag 6
	createPDU(pduBuffer, &sender_len, 1, &lastIndex);  // sender length
	createPDUWithChar(pduBuffer, myHandle, sender_len, &lastIndex); // sender
	createPDU(pduBuffer, &handle_count, 1, &lastIndex);  // number of handles
	for (int i = 0; i<handle_count; i++) {
		int index = i+2;  // offset for the handles
		char handleName[100];
		getUserInputBlock(userInput, index, handleName);
		uint8_t handle_length = strlen(handleName);
		createPDU(pduBuffer, &handle_length, 1, &lastIndex);  // handle length
		createPDUWithChar(pduBuffer, handleName, handle_length, &lastIndex);  // handle name
	}
	char message[MAXBUF];
	getUserInputMessage(userInput, 2 + handle_count, message);
	int message_length = strlen(message);
	batchSend(serverSocket, pduBuffer, message_length, message, lastIndex); // send in batch of 200 chars messages
}

void sendBroadcast(int serverSocket, uint8_t * userInput, char * myHandle) {
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 4;  // broadcast flag
	int lastIndex = 0;
	createPDU(pduBuffer, &flag, 1, &lastIndex);  // flag 4
	
	uint8_t sender_len = strlen(myHandle);
	createPDU(pduBuffer, &sender_len, 1, &lastIndex);  // sender length
	createPDUWithChar(pduBuffer, myHandle, sender_len, &lastIndex); // sender

	char message[MAXBUF];
	getUserInputMessage(userInput, 1, message);
	int message_length = strlen(message);
	batchSend(serverSocket, pduBuffer, message_length, message, lastIndex);
}

void sendList(int serverSocket, uint8_t * userInput, char * myHandle) {
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 10;  // broadcast flag
	int lastIndex = 0;
	createPDU(pduBuffer, &flag, 1, &lastIndex);  // flag 10
	int data_length = lastIndex;  // last index marks the size of the data
	sendPDU(serverSocket, pduBuffer, data_length);
}

void receiveFlag(int flag, uint8_t * dataBuffer, int * listMode) {
	if (flag == 5 || flag == 6) {  // uni and multi cast
		receiveUniMultiCast(dataBuffer);
	}
	else if (flag == 4) {  // broadcast
		receiveBroadcast(dataBuffer);
	}
	else if (flag == 7) {  // error handle not found
		receiveHandleNotFound(dataBuffer);
	}
	else if (flag == 11) {  // list (handle count)
		receiveListHandleCount(dataBuffer);
	}
	else if (flag == 12) {  // list (handle name)
		receiveListHandleName(dataBuffer);
	}
	else if (flag == 13)  {  // list is done
		*listMode = 0;
	}
}

/* Got uni or multi cast message from server */
void receiveUniMultiCast(uint8_t * dataBuffer) {
	int lastIndex = 1;
	char sender[100];
	getSender(dataBuffer, sender, &lastIndex);
	char message[200];
	getMessageFromPDU(dataBuffer, message, &lastIndex);
	printf("\n%s: %s\n", sender, message);
}

/* Recipient handle not found */
void receiveHandleNotFound(uint8_t * dataBuffer) {
	int lastIndex = 1;
	uint8_t handle_length;
	getFromPDU(dataBuffer, &handle_length, 1, &lastIndex);
	char handle_name[100];
	getFromPDUWithChar(dataBuffer, handle_name, handle_length, &lastIndex);
	printf("\nClient with handle %s does not exist\n", handle_name);
}

void receiveBroadcast(uint8_t * dataBuffer) {
	int lastIndex = 1;
	char sender[100];
	getSender(dataBuffer, sender, &lastIndex);
	char message[200];
	copyMessage(dataBuffer, lastIndex, message);
	printf("\n%s: %s\n", sender, message);
}

void receiveListHandleCount(uint8_t * dataBuffer) {
	int handle_count = dataBuffer[1];
	printf("Number of clients: %d\n", handle_count);
}

void receiveListHandleName(uint8_t * dataBuffer) {
	int lastIndex = 1;
	uint8_t handle_len;
	getFromPDU(dataBuffer, &handle_len, 1, &lastIndex);
	char handleName[100];
	getFromPDUWithChar(dataBuffer, handleName, handle_len, &lastIndex);
	handleName[handle_len] = '\0';
	printf("  %s\n", handleName);
}

/* Split user input by spaces and get string at index i
	Example: %M Timmy Message
	Timmy would be at index 1
 */
void getUserInputBlock(uint8_t * string, int index, char * buffer) {
	int spaceCount = 0;
	int i = 0;
	while (spaceCount != index){
		if (string[i] == ' ') {
			spaceCount++;
		}
		i++;
	}
	int j = 0;
	while (string[i] != ' ') {
		buffer[j] = string[i];
		i++;
		j++;
	}
	buffer[j] = '\0';
}


/* Gets the tail message of user input */
void getUserInputMessage(uint8_t * string, int index, char * buffer) {
	int spaceCount = 0;
	int i = 0;
	while (spaceCount != index){
		if (string[i] == ' ') {
			spaceCount++;
		}
		i++;
	}
	int j = 0;
	while (string[i] != '\0') {
		buffer[j] = string[i];
		i++;
		j++;
	}
	buffer[j] = '\0';
}

/* Get sender information for uni and multi cast */
void getSender(uint8_t * dataBuffer, char * senderBuffer, int * lastIndex) {
	uint8_t sender_length;
	getFromPDU(dataBuffer, &sender_length, 1, lastIndex);
	getFromPDUWithChar(dataBuffer, senderBuffer, sender_length, lastIndex);
	senderBuffer[sender_length] = '\0';
}

/* Get message from the tail of PDU */
void getMessageFromPDU(uint8_t * dataBuffer, char * message, int * lastIndex) {
	// Skip all the recipient data
	uint8_t handle_count;
	getFromPDU(dataBuffer, &handle_count, 1, lastIndex);
	for (int i = 0; i < handle_count; i++){
		uint8_t handle_length;
		getFromPDU(dataBuffer, &handle_length, 1, lastIndex);
		*lastIndex = *lastIndex + handle_length;
	}
	copyMessage(dataBuffer, *lastIndex, message);
}

/* Copies message into buffer given a starting index for dataBuffer */
void copyMessage(uint8_t * dataBuffer, int lastIndex, char * message) {
	int i = 0;
	int j = lastIndex;
	while (dataBuffer[j] != '\0'){
		message[i] = dataBuffer[j];
		i++;
		j++;
	}
	message[i] = '\0';
}

/* Sends messages to server in batches of at most 200 characters */
void batchSend(int serverSocket, uint8_t * data_buffer, int message_length, char * message, int lastIndex) {
	int headerLastIndex = lastIndex;
	int batchCount = 0;
	while (message_length > 0) {  // send in batches of 200 chars
		int sending_length;
		if (message_length > 199) {
			sending_length = 199;
		}
		else {
			sending_length = message_length;
		}
		char shippingMessage[200];
		memcpy(shippingMessage, message + batchCount * 199, sending_length);
		shippingMessage[sending_length] = '\0';

		createPDUWithChar(data_buffer, shippingMessage, sending_length + 1, &lastIndex);  // message
		int data_length = lastIndex;  // last index marks the size of the data
		sendPDU(serverSocket, data_buffer, data_length);
		message_length = message_length - sending_length;
		lastIndex = headerLastIndex;
		batchCount++;
	}
}