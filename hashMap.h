typedef struct Node{
	char * key;
	char * value;
	struct Node *next;
}Node;

Node * createNode(char * key, int value);

typedef struct Hashmap{
	int capacity;
	Node ** array;
	int size;
}HashMap;

int hash(char *str);

HashMap newHashMap();

void addToMap(HashMap *hMap, Node * n);

int getFromMap(HashMap *hMap, char * key);

void printLinkedList(Node *n);

void printHashMap(HashMap hMap);

void freeNodes(HashMap *hMap);


