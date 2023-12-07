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

//登入的過程不需修改
#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

//---------------------------------------------------------
/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
//-------這部分是在處理訊息查找----------
struct record_node{
	char name[32];
	char message[1000];
};
struct record_arr{
	int msg_ct;
	struct record_node record_arr[500];
};
//-------------------------------------
struct record_arr record_array;
//--------------------


void str_overwrite_stdout() {//沒用
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

int isSubstring(const char *str1, const char *str2) {
    int len1 = strlen(str1);
    int len2 = strlen(str2);

    for (int i = 0; i <= len2 - len1; ++i) {
        int j;
        for (j = 0; j < len1; ++j) {
            if (str2[i + j] != str1[j]) {
                break;
            }
        }
        if (j == len1) {
            return 1; 
        }
    }
    return 0; 
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
//被傳出去的字串
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

void send_message_to_specific_user(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void computeLPSArray(const char *pat, int M, int *lps) {
    int len = 0;
    lps[0] = 0;
    int i = 1;

    while (i < M) {
        if (pat[i] == pat[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

// KMP演算法的主函數，用於在單個文本str中搜索模式pat
void KMPSearch(const char *pat, const struct record_node node,int uid) {
    int M = strlen(pat);
    int N = strlen(node.message);

    int *lps = (int *)malloc(sizeof(int) * M);
    int j = 0;

    computeLPSArray(pat, M, lps);

    int i = 0;
    while (i < N) {
        if (pat[j] == node.message[i]) {
            j++;
            i++;
        }

        if (j == M) {
            //printf("%s\n",node.message);
			char to_user[100]= {'\0'};
			strcat(to_user,node.name);
			strcat(to_user,": ");
			strcat(to_user,node.message);
			strcat(to_user,"\n");
			printf("to_user : %s",to_user);
			send_message_to_specific_user(to_user, uid);
            break;
        } else if (i < N && pat[j] != node.message[i]) {
            if (j != 0)
                j = lps[j - 1];
            else
                i++;
        }
    }

    free(lps);
}

// 在字串陣列中進行KMP演算法的查找
void findSubstringInArray(struct record_arr obj, int size, const char *substr,int uid) {
    for (int i = 0; i < size; ++i) {
        //KMPSearch(substr, obj.record_arr[i].message,uid);
		KMPSearch(substr, obj.record_arr[i],uid);
		//send_message_to_specific_user(temp, uid);
    }
}

void send_private_message(char *s, int receiver_uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid==receiver_uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

int checkColonBeforeGreater(char* str) {
	printf("str in checkColonBeforeGreater %s\n",str);
    int foundColon = 0;
    int foundGreater = 0;

    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] == ':') {
            foundColon = 1;
        } else if (str[i] == '>') {
            foundGreater = 1;
            if (foundColon==0) return 0; // 如果 '>' 出現在 ':' 前面，回傳0
        }
    }
    if (foundColon==1&&foundGreater==1)return 1;
	else return 0;
}

void removeLeadingSpaces(char* str) {
    int i, j = 0, len;
    len = strlen(str);
    for (i = 0; i < len; i++) {// 遍歷字串找到第一個非空格字符的位置
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n' && str[i] != '\r') {
            break;
        }
    }
    for (j = 0; j < len - i; j++) {// 將非空格字符移到字串開頭
        str[j] = str[i + j];
    }
    str[j] = '\0'; // 在新的開頭位置後添加結尾符號
}
/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];
	char name[32];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);//sprintf 是一個字符串格式化函數，它允许你將多個值（在這種情況下，cli->name）轉換為一個格式化的字符串並將結果存儲在一個字符串中。
		printf("%s", buff_out);//buff_out 是一個字符數組（或字符串緩衝區），它用來存儲格式化後的字符串。
		send_message(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ);

	while(1){
		if (leave_flag) {
			break;
		}
		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){//--------------------------------------
				//printf("The current buffouts: %s\n",buff_out);
				int find_member_bool,private_msg_bool;
				char name[100],value[1000],private_str[1000],to_whom[1000],msg[1000],private_msg[2000],find_str[1000],sub_tar[100];
				sscanf(buff_out,"%[^:]:%[^\n]", name, value);
				removeLeadingSpaces(value);
				sscanf(value,"%[^:]:%[^\n]",find_str,sub_tar);
				if(strcmp(value,"member")==0)find_member_bool=1;
				else find_member_bool=0;
				if(checkColonBeforeGreater(value)){
					// printf("here\n");
					sscanf(value, "%[^:]:%[^>]>%[^:]s",private_str, to_whom,msg);
					//printf("private_str: %s\n",private_str);
					//printf("to_whom: %s\n", to_whom);
					//printf("msg: %s\n",msg);
					snprintf(private_msg, 1000, "Private message from ");
					strncat(private_msg, cli->name, 2000-strlen(private_msg)-strlen(cli->name));
					strncat(private_msg,":", 2000-strlen(private_msg)-1);
					strncat(private_msg,msg, 2000-strlen(private_msg)-strlen(msg));
					printf("private_msg: %s\n",private_msg);
					if(strcmp(private_str,"private")==0)private_msg_bool=1;
					else private_msg_bool=0;
				}
				else private_msg_bool=0;
				if(strcmp(find_str,"find")==0){
					printf("in find_msg\n");
					removeLeadingSpaces(sub_tar);
					findSubstringInArray(record_array,record_array.msg_ct ,sub_tar, cli->uid);
				}
				else if(private_msg_bool==1){
					int receiver_uid=-1;
					for (int i = 0; i < cli_count; i++) {
						if(strcmp(clients[i]->name,to_whom)==0){
							receiver_uid=clients[i]->uid;
							break;
						}
					}
					int length = strlen(private_msg);
					sprintf(private_msg + length, "\n"); // 在字串尾端加上換行符號
					if(receiver_uid==-1)send_message_to_specific_user("the user not online\n", cli->uid);
					else send_private_message(private_msg,receiver_uid);
				}
				else if (find_member_bool==1) {//查詢當前上線的成員
					char temp[BUFFER_SZ];
					snprintf(temp, sizeof(temp), "Online member: ");
					for (int i = 0; i < cli_count; i++) {// 這裡會先複製 "Online member: "，然後進行串接
						strncat(temp, clients[i]->name, sizeof(temp) - strlen(temp) - 1);
						if (i < cli_count - 1) {
							strncat(temp, ", ", sizeof(temp) - strlen(temp) - 1);
						}
						str_trim_lf(temp, strlen(temp));
					}
					int length = strlen(temp);
					if (temp[length - 1] != '\n') sprintf(temp + length, "\n"); // 在字串尾端加上換行符號
					send_message_to_specific_user(temp, cli->uid);
				}//--------------------------------------------------
				else{//無特殊指令時會加入訊息的儲存array

					strcpy(record_array.record_arr[record_array.msg_ct].message,value);
					strcpy(record_array.record_arr[record_array.msg_ct].name,name);
					record_array.msg_ct=(record_array.msg_ct+1)%500;
					send_message(buff_out, cli->uid);
					str_trim_lf(buff_out, strlen(buff_out));
					printf("%s \n", buff_out);
				}
			}
		} 
		else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} 
		else {
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
//server本身是不會關閉的
int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	record_array.msg_ct=0;
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
	signal(SIGPIPE, SIG_IGN);

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
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}