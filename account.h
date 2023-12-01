#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_SIZE 32
#define PASSWORD_SIZE 32
#define TABLE_SIZE 10

#define ACCOUNT_SIZE 50
#define NAME_SIZE 32
#define TABLE_SIZE 10
#define REQUEST_SIZE 100
#define RESPONSE_SIZE 100

typedef struct USER {
    char username[NAME_SIZE];
    char passward[PASSWORD_SIZE];
    struct USER *next;
} USER;

typedef struct HashTable {
    USER *table[TABLE_SIZE];
} HashTable;

// 創建一個新的節點
USER *createNode(const char *name, const char *pw) {
    USER *newNode = (USER *)malloc(sizeof(USER));
    if (newNode != NULL) {
        strcpy(newNode->username, name);
        strcpy(newNode->passward, pw);
        newNode->next = NULL;
    }
    return newNode;
}

void initHashTable(HashTable *hashTable) {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        hashTable->table[i] = NULL;
    }
}

//HashFunction
int hashFunction(const char *name) {
    int hash = 0;
    for (int i = 0; name[i] != '\0'; ++i) {
        hash += name[i];
    }
    return hash % TABLE_SIZE;
}

void insertItem(HashTable *hashTable, const char *name, const char *password) {
    int index = hashFunction(name);
    USER *newNode = createNode(name, password);
    
    //Linked List
    newNode->next = hashTable->table[index];
    hashTable->table[index] = newNode;
}


int loginUser (const HashTable *hashTable, const char *name, const char *password){
    int index = hashFunction(name);
    
    USER *pointer = hashTable->table[index];
    
    // 在Linked List中查找
    while (pointer != NULL) {
        if (strcmp(pointer->username, name) == 0) {
            if(strcmp(pointer->passward, password) == 0){
                return 1;
            }
            // 密碼錯了
            return 0;
        }
        pointer = pointer->next;
    }
    //找不到
    return 0;  
}

void freeHashTable(HashTable *hashTable) {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        USER *current = hashTable->table[i];
        while (current != NULL) {
            USER *next = current->next;
            free(current);
            current = next;
        }
        hashTable->table[i] = NULL;
    }
}