#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>

#define LENGTH 2048

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout() {
  printf("%s", "> ");
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

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}
//等三秒再判斷
int wait_for_string_input(const char *target) {
    char input[LENGTH];            // 定義一個緩沖區來存儲用戶輸入的字符串
    fd_set rfds;                   // 定義文件描述符集合，用於 select 函數
    struct timeval tv;             // 定義時間結構體，用於設置 select 的等待時間
    int retval;                    // 用於存儲 select 函數的返回值

    FD_ZERO(&rfds);                // 清空文件描述符集合
    FD_SET(STDIN_FILENO, &rfds);   // 將標準輸入文件描述符添加到集合中

    tv.tv_sec = 3;                 // 設置等待時間為3秒
    tv.tv_usec = 0;

    retval = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    // 使用 select 函數等待標準輸入是否有可讀事件

    if (retval == -1) {
        perror("select()");         // 如果 select 返回 -1，表示出錯，打印錯誤信息
        return 0;                   // 返回 0，表示出錯
    } else if (retval) {
        // 如果 retval 大於 0，表示標準輸入有可讀事件
        fgets(input, LENGTH, stdin);  // 從標準輸入讀取用戶輸入的字符串
        str_trim_lf(input, LENGTH);   // 移除可能的換行符

        return strcmp(input, target) == 0; // 檢查輸入是否與目標字符串相同
    } else {
        // 如果 retval 等於 0，表示超時，沒有輸入
        return 0; // 返回 0，表示沒有輸入
    }
}

// void send_msg_handler() {
//     char message[LENGTH] = {};
//     char buffer[LENGTH + 64] = {}; // 增加缓冲区大小
//     //printf("in send_msg\n");
//     while (1) {
//         str_overwrite_stdout();
//         fgets(message, LENGTH, stdin);
//         str_trim_lf(message, LENGTH);
//         //printf("in while\n");
//         if (strcmp(message, "exit") == 0) {
//             break;
//         } else {
//             // 使用snprintf代替sprintf
//             snprintf(buffer, sizeof(buffer), "%s: %s\n", name, message);
//             //printf("now sending\n");
//             send(sockfd, buffer, strlen(buffer), 0);
//         }

//         bzero(message, LENGTH);
//         bzero(buffer, sizeof(buffer)); // 清空整个缓冲区
//     }
//     catch_ctrl_c_and_exit(2);
// }

void send_msg_handler() {
    char message[LENGTH] = {};
    char buffer[LENGTH + 64] = {};

    while (1) {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);
        if (strcmp(message, "exit") == 0)
        {
          break;
        } 
        else 
        {
          if (wait_for_string_input("return")) 
          {
            continue;
          }
          else
          {
            snprintf(buffer, sizeof(buffer), "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
          } 
        }
          bzero(message, LENGTH);
          bzero(buffer, sizeof(buffer));
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
	char message[LENGTH] = {};
  while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0) 
    {
      if (strcmp(message, "blacklist") == 0) {
          printf("You are in the blacklist. Exiting...\n");
          catch_ctrl_c_and_exit(2);  // 结束客户端
      } else {
          printf("%s", message);
          str_overwrite_stdout();
      }
      //printf("%s", message);
      //str_overwrite_stdout();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter your name: ");
  fgets(name, 32, stdin);
  str_trim_lf(name, strlen(name));


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);


  // Connect to Server
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(flag){
			printf("\nBye\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}

