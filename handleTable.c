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


HandleTable newHandleTable(){
    char ** handleArray = (char**)malloc(sizeof(char*)*10);
    int * socketArray = (int*)malloc(sizeof(int)*10);

    HandleTable table = {handleArray, socketArray, 10, 0};
    return table;
}

void addToTable(HandleTable *table, char * handle, int socket) {
    // Exceed capacity, realloc needed
    if (table -> size == table -> capacity) {
        table -> capacity = table -> capacity * 2;
        table -> handleArray = (char **) realloc(table -> handleArray, sizeof(char*) * table -> capacity);
        table -> socketArray = (int *) realloc(table -> socketArray, sizeof(int) * table -> capacity);
    }
    
    // add to back of array
    char ** handleArray = table -> handleArray;
    int * socketArray = table -> socketArray;
    int lastIndex = table -> size;
    handleArray[lastIndex] = handle;
    socketArray[lastIndex] = socket;
    table->size = table->size + 1;
}

int getByHandle(HandleTable *table, char * handle) {
    char ** handleArray = table -> handleArray;
    int size = table -> size;
    for (int i = 0; i < size; i++) {
        if (strcmp(handle, handleArray[i]) == 0) {  // match found
            return table -> socketArray[i];
        }
    }
    return -1; // not found
}

char * getBySocket(HandleTable *table, int socket) {
    int * socketArray = table -> socketArray;
    int size = table -> size;
    for (int i = 0; i < size; i++) {
        if (socket == socketArray[i]) {  // match found
            return table -> handleArray[i];
        }
    }
    return "None"; // not found
}

void removeHandle(HandleTable *table, char * handle){
    char ** newHandleArray = (char **)malloc(sizeof(char*) * table -> capacity);
    int * newSocketArray = (int *)malloc(sizeof(int) * table -> capacity);
    int newIndex = 0;
    for (int i = 0; i < table->size; i++){
        if (strcmp(table->handleArray[i], handle) != 0) {  // not match
            newHandleArray[newIndex] = table->handleArray[i];
            newSocketArray[newIndex] = table->socketArray[i];
            newIndex += 1;
        }
    }
    free(table->handleArray);
    free(table->socketArray);
    table->handleArray = newHandleArray;
    table->socketArray = newSocketArray;
    table->size = newIndex;
}

void printTable(HandleTable *table){  // for debugging purposes
    for (int i = 0; i < table -> size; i++) {
        printf("%s,%d\n", table -> handleArray[i], table->socketArray[i]);
    }
}

void freeTable(HandleTable *table){
    free(table->handleArray);
    free(table->socketArray);
}
