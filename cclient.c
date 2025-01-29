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

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define FLAG_SIZE 1
#define HANDLE_LEN_SIZE 1

int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void checkServerTermination(int serverSocket);
void clientControl(int serverSocket, char * myHandle);
void processStdin(int serverSocket, char * myHandle);
void processMsgFromServer(int serverSocket);
void registerHandle(char * handle, int serverSocket);
void getStringBlock(uint8_t * string, int index, char * buffer);
void getStringMessage(uint8_t * string, int index, char * buffer);
void processUnicast(int serverSocket, uint8_t * userInput, char * myHandle);
void processMultiCast(int serverSocket, uint8_t * userInput, char * myHandle);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);
	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	char * myHandle = argv[1];
	registerHandle(myHandle, socketNum);
	clientControl(socketNum, myHandle);
	close(socketNum);
	return 0;
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

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
}

void clientControl(int serverSocket, char *myHandle){
	setupPollSet();
	addToPollSet(STDIN_FILENO);
	addToPollSet(serverSocket);
	while (1) {
		printf("$:");
		fflush(stdout);  // force $: out to stdout
		int activeSocket = pollCall(-1);
		if (activeSocket == STDIN_FILENO) {
			processStdin(serverSocket, myHandle);
		}
		else {
			processMsgFromServer(serverSocket);
		}
	}
}

void processMultiCast(int serverSocket, uint8_t * userInput, char * myHandle) {
	uint8_t pduBuffer[MAXBUF];
	uint8_t flag = 6;
	int lastIndex = 0;
	createPDU(pduBuffer, &flag, 1, &lastIndex);  // flag 6
	
	uint8_t sender_len = strlen(myHandle) + 1;
	createPDU(pduBuffer, &sender_len, 1, &lastIndex);  // sender length
	createPDUWithChar(pduBuffer, myHandle, sender_len, &lastIndex); // sender

	char handle_count_str[3];
	getStringBlock(userInput, 1, handle_count_str);
	uint8_t handle_count = atoi(handle_count_str);
	createPDU(pduBuffer, &handle_count, 1, &lastIndex);  // number of handles
	if (handle_count > 9) {
		printf("Handle count cannot be larger than 9");
		return;
	}

	for (int i = 0; i<handle_count; i++) {
		int index = i+2;
		char handleName[100];
		getStringBlock(userInput, index, handleName);
		uint8_t handle_length = strlen(handleName) + 1;
		createPDU(pduBuffer, &handle_length, 1, &lastIndex);  // handle length
		createPDUWithChar(pduBuffer, handleName, handle_length, &lastIndex);  // handle name
	}
	char message[200];
	getStringMessage(userInput, 2 + handle_count, message);
	uint8_t message_length = strlen(message) + 1;
	createPDUWithChar(pduBuffer, message, message_length, &lastIndex);  // message
	int data_length = lastIndex;  // last index marks the size of the data
	sendPDU(serverSocket, pduBuffer, data_length);
}

void processStdin(int serverSocket, char * myHandle){
	uint8_t userInput[MAXBUF]; 	
	readFromStdin(userInput);
	if (userInput[0] == '%') {
		if (userInput[1] == 'M' || userInput[1] == 'm') {
			processUnicast(serverSocket, userInput, myHandle);
		}
		else if (userInput[1] == 'C' || userInput[1] == 'c') {
			processMultiCast(serverSocket, userInput, myHandle);
		}
		else {
			printf("\n%c is an invalid command, only M, C, B and L are accepted\n", userInput[1]);
		}
	}
	else {
		printf("Invalid Input, must start with percentage\n");
	}
}

void getSender(uint8_t * dataBuffer, char * senderBuffer, int * lastIndex) {
	uint8_t sender_length;
	getFromPDU(dataBuffer, &sender_length, 1, lastIndex);
	getFromPDUWithChar(dataBuffer, senderBuffer, sender_length, lastIndex);
}

void getMessage(uint8_t * dataBuffer, char * message, int * lastIndex) {
	// Skip all the recipient data
	uint8_t handle_count;
	getFromPDU(dataBuffer, &handle_count, 1, lastIndex);
	for (int i = 0; i < handle_count; i++){
		uint8_t handle_length;
		getFromPDU(dataBuffer, &handle_length, 1, lastIndex);
		*lastIndex = *lastIndex + handle_length;
	}

	int i = 0;
	int j = *lastIndex;
	while (dataBuffer[j] != '\0'){
		message[i] = dataBuffer[j];
		i++;
		j++;
	}
	message[i] = '\0';
}

void receiveUniMultiCast(uint8_t * dataBuffer) {
	int lastIndex = 1;
	char sender[100];
	getSender(dataBuffer, sender, &lastIndex);
	char message[200];
	getMessage(dataBuffer, message, &lastIndex);
	printf("\n%s: %s\n", sender, message);
}

void processHandleNotFound(uint8_t * dataBuffer) {
	int lastIndex = 1;
	uint8_t handle_length;
	getFromPDU(dataBuffer, &handle_length, 1, &lastIndex);
	char handle_name[100];
	getFromPDUWithChar(dataBuffer, handle_name, handle_length, &lastIndex);
	printf("\nClient with handle %s does not exist\n", handle_name);
}

void processFlag(int flag, uint8_t * dataBuffer) {
	if (flag == 5 || flag == 6) {  // uni and multi cast
		receiveUniMultiCast(dataBuffer);
	}
	else if (flag == 7) {  // error handle not found
		processHandleNotFound(dataBuffer);
	}
}

void processMsgFromServer(int serverSocket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(serverSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		int flag = dataBuffer[0];
		processFlag(flag, dataBuffer);
	}
	else
	{
		printf("Server has terminated\n");
		exit(0);
	}
}

void registerHandle(char * handle, int serverSocket){
	uint8_t handle_length = strlen(handle) + 1;
	if (handle_length > 100) {
		printf("Handle Length cannot be longer than 100 characters");
		exit(0);
	}
	uint8_t flag = 1;
	uint8_t data_buffer[MAXBUF];
	memcpy(data_buffer, &flag, sizeof(uint8_t));
	memcpy(data_buffer + 1, &handle_length, sizeof(uint8_t));
	memcpy(data_buffer + 2, handle, sizeof(uint8_t) * handle_length);
	int data_length = FLAG_SIZE + HANDLE_LEN_SIZE + handle_length;
	int sent = sendPDU(serverSocket, data_buffer, data_length);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
	
	uint8_t responseBuffer[MAXBUF];
	int messageLen = 0;
	//check responce from server for registration
	if ((messageLen = recvPDU(serverSocket, responseBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		if (responseBuffer[0] == 2) {  // flag 2
			printf("Handle %s Registered Successfully\n", handle);
		} 
		else if (responseBuffer[0] == 3) {  // flag 3
			printf("Handle %s registration failed because it already exist\n", handle);
		}
		else {
			printf("Response flag %d unrecognized\n", responseBuffer[0]);
		}
	}
	else
	{
		printf("Server has terminated\n");
		exit(0);
	}
}

void getStringBlock(uint8_t * string, int index, char * buffer) {
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

void getStringMessage(uint8_t * string, int index, char * buffer) {
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

void processUnicast(int serverSocket, uint8_t * userInput, char * myHandle) {
	char handleName[100];
	char message[200];
	getStringBlock(userInput, 1, handleName);
	getStringMessage(userInput, 2, message);
	uint8_t handle_length = strlen(handleName) + 1;
	uint8_t message_length = strlen(message) + 1;
	uint8_t myHandle_length = strlen(myHandle) + 1;
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
	createPDUWithChar(data_buffer, message, message_length, &lastIndex);  // message
	int data_length = lastIndex;  // last index marks the size of the data
	sendPDU(serverSocket, data_buffer, data_length);
}