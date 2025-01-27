#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


typedef struct HandleTable {
    char ** handleArray;
    int * socketArray;
    int capacity;
    int size;
} HandleTable;

HandleTable newHandleTable();
void addToTable(HandleTable *table, char * handle, int socket);
void removeHandle(HandleTable *table, char * handle);
int getByHandle(HandleTable *table, char * handle);
char * getBySocket(HandleTable *table, int socket);
void printTable(HandleTable *table);
void freeTable(HandleTable *table);
