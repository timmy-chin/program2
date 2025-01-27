#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct Node{
	char * key;
	int value;
	struct Node *next;
}Node;

// Creates a node by mallocing space and populating it
Node * createNode(char * key, int value){
	Node * n = (Node*)malloc(sizeof(Node));
	n -> key = key;
	n -> value = value;
	n -> next = NULL;
	return n;
}


typedef struct Hashmap{
	int capacity;
	Node ** array;
	int size;
}HashMap;


// Hashes a key with its ASCII
int hash(char *str){
	uint32_t hashCode = 1;
	int n = 0;
	while (str[n] != '\0'){
		hashCode = hashCode * 31 + str[n];
		hashCode = hashCode % 2147483647; // prevent sign overflow
		n++;
	}
	return hashCode;

}


// constructor for hashMap
HashMap newHashMap(){
	Node ** nodeArray = (Node**)malloc(sizeof(Node*)*10);
	
	// Null all entries
	for (int i = 0; i < 10; i++){
		nodeArray[i] = NULL;
	}
	
	HashMap hMap = {10, nodeArray, 0};
	return hMap;
}

// add to the map by hashing
void addToMap(HashMap *hMap, Node * n){

	// Rehash
	
	if (hMap->size == hMap->capacity){
		int oldCapacity = hMap->capacity;
		hMap->capacity = hMap->capacity*2;
	
		Node** nodeArray = (Node**)malloc(sizeof(Node*)*hMap->capacity);

		// Null all entries
		for (int i = 0; i < hMap->capacity; i++){
			nodeArray[i] = NULL;
		}

		HashMap newHMap = {hMap->capacity, nodeArray, 0};

		for (int i = 0; i < oldCapacity; i++){
			Node * entry = hMap->array[i];
			while (entry != NULL){
				Node * next = entry -> next;
				entry -> next = NULL;
				addToMap(&newHMap, entry); // put node into new hashMap
				entry = next;

			}
		}
		free(hMap->array);
		hMap->array = newHMap.array; // set to new map

	}

	int hashCode = hash(n->key);
	int hashIdx = hashCode % hMap->capacity; // get hash idx

	Node *current = hMap->array[hashIdx];
	if (current == NULL){ // if first node in idx
		hMap->array[hashIdx] = n;
	}
	else{
		Node * prev = NULL;
		while (current != NULL){ // traverse linkedlist
			if (strcmp(n->key,current->key) == 0){ // if overwriting key
				current->value = n->value;
				free(n->key);
				free(n);
				return;
			}

			prev = current;
			current = current -> next;

		}
		prev -> next = n;
		
	}
	hMap->size++;
	
}

int getFromMap(HashMap *hMap, char * key){
	int hashCode = hash(key);
	int hashIdx = hashCode % hMap->capacity; // get hash idx

	Node * current = hMap->array[hashIdx];
	while (current != NULL){ // traverse until key found
		if (strcmp(key, current->key) == 0){
			return current -> value;
		}
		current = current -> next;
	}

	return -1; // none was found
}




void printLinkedList(Node *n){ // for debugging purposes
	if (n != NULL){
		printf("(%s, %d)->", n->key, n->value);
		printLinkedList(n->next);
	}


}


void printHashMap(HashMap hMap){ // for debugging purposes
	for (int i = 0; i<hMap.capacity; i++){
		Node * current = hMap.array[i];
		if (current != NULL){
			printf("%d: ", i);
			printLinkedList(current);
			printf("\n");
		}
		else{
			printf("%d: NULL\n", i);
		}

	}
}




void freeNodes(HashMap *hMap){ // frees all the keys and nodes in map
	for (int i = 0; i < hMap->capacity; i++){
		Node * n = hMap->array[i];
		while (n!= NULL){
			Node * current = n;
			n = n->next;
			free(current->key);
			free(current);
		}
	}
}





