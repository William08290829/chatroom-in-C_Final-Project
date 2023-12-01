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
#include <malloc.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define MAX_NAME_LENGTH 32
#define MAX_BLACKLIST_SIZE 10

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[MAX_NAME_LENGTH];
} client_t;

typedef struct blacklist {
    int room;
    char blacklist_name[MAX_BLACKLIST_SIZE][MAX_NAME_LENGTH];
    struct blacklist *next;
} Blacklist;

Blacklist *head = NULL;

void createNode(int room, char names[][MAX_NAME_LENGTH], int count) {
    Blacklist *newNode = (Blacklist *)malloc(sizeof(Blacklist));
    if (newNode != NULL) {
        newNode->room = room;
        newNode->next = NULL;

        for (int i = 0; i < count; i++) {
            if (i < MAX_BLACKLIST_SIZE) {
                strcpy(newNode->blacklist_name[i], names[i]);
            } else {
                // Handle the case where the blacklist is full
                fprintf(stderr, "Blacklist is full. Some names were not added.\n");
                break;
            }
        }

        if (head == NULL) {
            head = newNode;  // If the list is empty, make the new node the head
        } else {
            // Traverse to the end of the list
            Blacklist *ptr = head;
            while (ptr->next != NULL) {
                ptr = ptr->next;
            }

            // Add the new node to the end of the list
            ptr->next = newNode;
        }
    }
}

int findnode(Blacklist* head, int room, char *name) {
    int a = 0;
    Blacklist* ptr = head;
    while (ptr != NULL) {
        if (room != ptr->room) {
            ptr = ptr->next;
        } else {
            a = 1;
            break;  // Add a break statement to exit the loop when room is found
        }
    }
    if (a == 1) {
        for (int i = 0; i < 6; i++) {
            if (strlen(ptr->blacklist_name[i]) > 0 && strcmp(ptr->blacklist_name[i], name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}


client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
// 全局變量，用於文件操作
FILE *logfile;

// 清除文件内容
void clear_file() {
    FILE *clearFile = fopen("chat.txt", "w");
    if (clearFile == NULL) {
        perror("ERROR: Could not open file for clearing");
        exit(EXIT_FAILURE);
    }
    fclose(clearFile);
}

// 初始化文件，打開文件寫入
void init_file() {
	//清除文件
	clear_file();
	pthread_mutex_lock(&file_mutex);  // 加鎖
    logfile = fopen("chat.txt", "a");
    if (logfile == NULL) {
        perror("ERROR: Could not open log file");
        exit(EXIT_FAILURE);
    }
	pthread_mutex_unlock(&file_mutex);  // 解鎖
}

// 寫入消息到文件
void write_to_file(const char *message) {
	pthread_mutex_lock(&file_mutex);  // 加鎖
    if (logfile != NULL) {
        fprintf(logfile, "%s", message);
        fflush(logfile); // 立即寫入文件
    }
	pthread_mutex_unlock(&file_mutex);  // 解鎖
}

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
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
	pthread_mutex_lock(&clients_mutex);

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

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
// void *handle_client(void *arg){
// 	char buff_out[BUFFER_SZ];
// 	char name[32];
// 	int leave_flag = 0;

// 	cli_count++;
// 	client_t *cli = (client_t *)arg;

// 	// Name
// 	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
// 		printf("Didn't enter the name.\n");
// 		leave_flag = 1;
// 	} else{
// 		strcpy(cli->name, name);
// 		sprintf(buff_out, "%s has joined\n", cli->name);
// 		printf("%s", buff_out);
// 		send_message(buff_out, cli->uid);
// 	}

// 	bzero(buff_out, BUFFER_SZ);

// 	while(1){
// 		if (leave_flag) {
// 			break;
// 		}

// 		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
// 		if (receive > 0){
// 			if(strlen(buff_out) > 0){
// 				send_message(buff_out, cli->uid);

// 				str_trim_lf(buff_out, strlen(buff_out));
// 				//printf("%s -> %s\n", buff_out, cli->name);
// 				printf("%s\n", buff_out);
// 			}
// 		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
// 			sprintf(buff_out, "%s has left\n", cli->name);
// 			printf("%s", buff_out);
// 			send_message(buff_out, cli->uid);
// 			leave_flag = 1;
// 		} else {
// 			printf("ERROR: -1\n");
// 			leave_flag = 1;
// 		}

// 		bzero(buff_out, BUFFER_SZ);
// 	}

//   /* Delete client from queue and yield thread */
// 	close(cli->sockfd);
//   queue_remove(cli->uid);
//   free(cli);
//   cli_count--;
//   pthread_detach(pthread_self());

// 	return NULL;
// }
int room=0;
/* Handle all communication with the client */
void *handle_client(void *arg){

    char buff_out[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;
    cli_count++;
    client_t *cli = (client_t *)arg;

    // Name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32-1){
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    } 
	else{
		if (findnode(head, room, name) == 0)
		{
			strcpy(cli->name, name);
			sprintf(buff_out, "%s has joined\n", cli->name);
			printf("%s", buff_out);
			// Write the joined message to the file
			fprintf(logfile, "%s has joined\n", cli->name);
			fflush(logfile);
			send_message(buff_out, cli->uid);
		}
		else
		{
			strcpy(cli->name, name);
			sprintf(buff_out, "%s is in blacklist, so he/she can't login in\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			send(cli->sockfd, "blacklist", strlen("blacklist"), 0);
            leave_flag = 1;
		}

    }

    bzero(buff_out, BUFFER_SZ);

    while(1){
        if (leave_flag) {
            break;
        }

        int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
        if (receive > 0){
            if (strlen(buff_out) > 0){
                send_message(buff_out, cli->uid);

                str_trim_lf(buff_out, strlen(buff_out));
                // Write the message to the file
                fprintf(logfile, "%s\n", buff_out);
                fflush(logfile);
                printf("%s\n", buff_out);
            }
        } else if (receive == 0 || strcmp(buff_out, "exit") == 0){
            sprintf(buff_out, "%s has left\n", cli->name);
            printf("%s", buff_out);
            // Write the left message to the file
            fprintf(logfile, "%s has left\n", cli->name);
            fflush(logfile);
            send_message(buff_out, cli->uid);
            leave_flag = 1;
        } else {
            printf("ERROR: -1\n");
            // Write the left message to the file
            fprintf(logfile, "%s has left\n", cli->name);
            fflush(logfile);
            leave_flag = 1;
        }

        bzero(buff_out, BUFFER_SZ);
    }

	/* Delete client from queue and yield thread */
	close(cli->sockfd);
	queue_remove(cli->uid);
	pthread_detach(pthread_self());
	free(cli);
	cli_count--;

    return NULL;
}


int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	char names_1[6][MAX_NAME_LENGTH] = {"John", "Doe", "Alice", "Bob", "Eve", "Charlie"};
	char names_2[6][MAX_NAME_LENGTH] = {"John", "Jim", "Alice", "Kevin", "Eason","William"};
	char names_3[6][MAX_NAME_LENGTH] = {"Enderson", "Doe", "Frank", "Bob","Kevin","Eason"};
	createNode(4444, names_1, 6);
	createNode(1111, names_2, 6);
	createNode(2222, names_3, 6);
	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	room=port;
	int option = 1;
	int listenfd = 0, connfd = 0;
  	struct sockaddr_in serv_addr;
  	struct sockaddr_in cli_addr;
  	pthread_t tid;
	//一般來說，address 的實際數值都會用 in_addr 或者 in_addr_t 來表示 其本質就是 uint32_t，用總共 32 個 bits 來表示一個 IPv4 的地址
  	/* Socket settings */
	/*domain
	定義了socket要在哪個領域溝通，常用的有2種：

	AF_UNIX/AF_LOCAL：用在本機程序與程序間的傳輸，讓兩個程序共享一個檔案系統(file system)
	AF_INET , AF_INET6 ：讓兩台主機透過網路進行資料傳輸，AF_INET使用的是IPv4協定，而AF_INET6則是IPv6協定。*/
  	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

	/* Ignore pipe signals */
		signal(SIGPIPE, SIG_IGN);
		//if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
		if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,(char*)&option,sizeof(option)) < 0){
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
	init_file();
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
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}
	// Close the file
	fclose(logfile);
	return EXIT_SUCCESS;
}


           
