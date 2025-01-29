#ifndef __PDUUTIL_H__
#define __PDUUTIL_H__

#include <stdint.h>

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData);
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize);
void getFromPDU(uint8_t * pduBuffer, uint8_t *buffer, int size, int * lastIndex);
void getFromPDUWithChar(uint8_t * pduBuffer, char * buffer, int size, int * lastIndex);
void createPDU(uint8_t * data_buffer, uint8_t * value, int size, int * lastIndex);
void createPDUWithChar(uint8_t * data_buffer, char * value, int size, int * lastIndex);

#endif