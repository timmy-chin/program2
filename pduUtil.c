#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "pduUtil.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXBUF 1024


int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData) {
    // check if message is too long
    if (lengthOfData > MAXBUF) {
        perror("message too long and can't find max buffer of 1024");
        return -1;
    }

    uint8_t pduBuffer[MAXBUF + 2];  // To store the entire PDU
    int pduLengthField = htons(lengthOfData + 2);  // The length field
    memcpy(pduBuffer, &pduLengthField, sizeof(uint16_t));  // Adding the length to the front of the buffer 
    memcpy(pduBuffer + 2, dataBuffer, lengthOfData * sizeof(uint8_t));  // Adding the entire message
    int bytesSent;
    if ((bytesSent = send(clientSocket, pduBuffer, lengthOfData + 2, 0)) < 0)
	{
        perror("send call\n");
        return -1;
     }
	 
    return lengthOfData;
}
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize) {
    uint8_t lenghtBuffer[2]; // Buffer for the length field in first recv
    int bytesReceived = recv(socketNumber, lenghtBuffer, 2, MSG_WAITALL); // recv only 2 bytes
    if (bytesReceived == 0)  
    {
            return 0; // close connection
    }
    else if (bytesReceived < 0)  
    {
        if (errno == ECONNRESET)
        {
            return 0; // close connection
        }
        perror("First PDU recv got an error\n");
        return -1;
    }

    int messageLen = lenghtBuffer[1] - 2; // Get length of meesage
    bytesReceived = recv(socketNumber, dataBuffer, messageLen, MSG_WAITALL); // recv the message length from stream
    if (bytesReceived < 0)  
    {
            perror("Second PDU recv got an error\n");
            return -1;
    }
    return bytesReceived;
}

void getFromPDU(uint8_t * pduBuffer, uint8_t * buffer, int size, int * lastIndex){
	memcpy(buffer, pduBuffer + *lastIndex, sizeof(uint8_t) * size);
	*lastIndex = *lastIndex + size;
}

void getFromPDUWithChar(uint8_t * pduBuffer, char * buffer, int size, int * lastIndex){
	memcpy(buffer, pduBuffer + *lastIndex, sizeof(char) * size);
	*lastIndex = *lastIndex + size;
}

void createPDU(uint8_t * data_buffer, uint8_t * value, int size, int * lastIndex) {
	memcpy(data_buffer + *lastIndex, value, sizeof(uint8_t) * size);  // flag
	*lastIndex = *lastIndex + size;
}

void createPDUWithChar(uint8_t * data_buffer, char * value, int size, int * lastIndex) {
	memcpy(data_buffer + *lastIndex, value, sizeof(uint8_t) * size);  // flag
	*lastIndex = *lastIndex + size;
}