#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include "account.h"

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define ACCOUNT_SIZE 50
#define NAME_SIZE 32
#define PASSWORD_SIZE 32
#define TABLE_SIZE 10
#define REQUEST_SIZE 100
#define RESPONSE_SIZE 100


static _Atomic unsigned int cli_count = 0;	//_Atomic原子性 不會被其他線程所打斷
static int uid = 10;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;	
	int uid;
	char name[NAME_SIZE];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;	//mutex互斥鎖

//NEW
typedef struct {
    HashTable *hashTable;
    client_t *client;
} ThreadArgs;

void str_overwrite_stdout() {
    printf("\r%s", "> ");	//\r:回車(覆蓋該行使光標回到開頭)
    fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // 去除字符串末尾的換行符(\n)
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);		//pthread.h提供的函數

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender寄件人 */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){	//如果不是寄件人的話
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){	//write為<unistd.h>的函數
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Account request
void handle_account_request(HashTable *hashTable, char *request, int sockfd, int uid){
	char *token = strtok(request, " ");
	if (token == NULL) {
        char message[] = "Request Fail.";
		send(sockfd, message, strlen(message), 0);
        return;
    }

	if (strcmp(token, "login") == 0) {
        // 登錄請求
        char *name = strtok(NULL, " ");
        char *password = strtok(NULL, " ");

        if (name != NULL && password != NULL) {
            if (loginUser(hashTable, name, password)) {		//存在這帳號
				char message[RESPONSE_SIZE] = "Login Success";
				send(sockfd, message, sizeof(message), 0);
            } 
			else {
                char message[RESPONSE_SIZE] = "Login Fail";
				send(sockfd, message, sizeof(message), 0);
            }
        } 
		else {
            char message[RESPONSE_SIZE] = "Request Fail.";
			send(sockfd, message, sizeof(message), 0);
        }
    }
	else if(strcmp(token, "create") == 0){
		char *name = strtok(NULL, " ");
        char *password = strtok(NULL, " ");

		if (name != NULL && password != NULL) {
			insertItem(hashTable, name, password);
			printf("An account has been created.\n[Name:%s Password:%s]\n", name, password);
			char response[RESPONSE_SIZE];
           	snprintf(response, RESPONSE_SIZE, "Hello!, %s\n", name);
			send(sockfd, response, sizeof(response), 0);
			return;
		}
		else {
            char message[] = "Create Fail";
			send(sockfd, message, sizeof(message), 0);
        }

	}
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];
	char name[32];
	char request[REQUEST_SIZE];
	int leave_flag = 0;

	ThreadArgs *args = (ThreadArgs *)arg;
    HashTable *hashTable = args->hashTable;

	cli_count++;
	client_t *cli = args->client;


	int receive = recv(cli->sockfd, request, REQUEST_SIZE, 0);
	if(receive > 0){ 	//成功接收
		handle_account_request(hashTable, request, cli->sockfd, cli->uid);
	}
	else{
		char message[] = "handle_account_request Fail.";
		send(cli->sockfd, message, strlen(message), 0);
	}

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){	//recv如果返回值是 0，表示對方已經關閉連接。如果返回值是 -1，表示發生錯誤
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);
		printf("%s", buff_out);	//在server上印出誰加入了
		send_message(buff_out, cli->uid);	//在client上印出誰加入了
	}

	bzero(buff_out, BUFFER_SZ);	//清空 buff_out的內存

	while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				// handle_account_request(&userAccountDatabase, buff_out, cli->sockfd, cli->uid);
				send_message(buff_out, cli->uid);

				str_trim_lf(buff_out, strlen(buff_out));
				printf("%s\n", buff_out);
				// printf("%s -> %s\n", buff_out, cli->name);
			}
		}
		else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		}
		else {	//recv回傳-1
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

  	/* Delete client from queue and yield thread */
	close(cli->sockfd);
  	queue_remove(cli->uid);
  	free(cli);
  	cli_count--;
  	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Accout Database
	HashTable userAccountDatabase;
	initHashTable(&userAccountDatabase);

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  	struct sockaddr_in serv_addr;
  	struct sockaddr_in cli_addr;
  	pthread_t tid;

  	/* Socket settings */
  	listenfd = socket(AF_INET, SOCK_STREAM, 0);
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr.s_addr = inet_addr(ip);
  	serv_addr.sin_port = htons(port);

  	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);	//為了防止由於向已經關閉的套接字寫入而終止程序

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    	return EXIT_FAILURE;
	}

	/* Bind */
  	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    	perror("ERROR: Socket binding failed");
    	return EXIT_FAILURE;
  	}

  	/* Listen */
  	if (listen(listenfd, 10) < 0) {
    perror("ERROR: Socket listening failed");
    return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */	
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		ThreadArgs threadArgs = {&userAccountDatabase, cli};
		pthread_create(&tid, NULL, &handle_client, (void*)&threadArgs);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}

           
