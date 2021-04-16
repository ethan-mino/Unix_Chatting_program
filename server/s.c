#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#define MAX_LEN    255
#define MAX_SOCK    512
#define MAX_USR 20   // 최대 사용자 수

struct room {   // 각 채팅방의 정보를 저장하는 구조체
	char room_name[MAX_LEN];   // 채팅방 이름
	char sock_name[MAX_LEN];   // 소켓 파일명
	char * user_list[MAX_USR];   // 사용자명 배열   	
	int client_s[MAX_SOCK];   // 클라이언트와 연결하는 소켓 디스크립터 배열
	int cli_cnt;   // 연결된 사용자 수
	int user_arg[MAX_USR];	//***** 추가 사용자 옵션
	pthread_t thread;   // thread identifier
};

struct pass {   // 스레드로 전달되는 구조체
	int psd;   // 부모 서버와 클라이언트를 연결하는 소켓 디스크립터
	struct room * cur_room;   // 현재 채팅방에 대한 구조체
};


void handler(int signo);
void ifexit();
int connect_cli(char * sock_name);
void get_room_name(int sockfd, char buf[], size_t len, int flags);
int move(int nsd);
void * chat_process(void * p);
void room_list_print(int psd);   // 채팅방 목록을 클라이언트에게 전달하는 함수

struct room room_list[20] = { 0 };   // 방의 정보를 저장하는 구조체로, 일단 최대 방의 개수를 20개로 설정.
int room_cnt = 0;   // room_cnt는 room의 개수, 전역 변수로 두어서 move 함수에서도 사용사고 수정할 수 있도록 함.
char *output[] = { "(%R) " };   // 시간 출력 형식
char SOCK_NAME[MAX_LEN];   // 클라이언트와 부모 서버를 연결하는데 사용되는 소켓 파일명

int main() {
	struct sockaddr_un cli;
	char buf[MAX_LEN], sender[MAX_LEN], chatfile[MAX_LEN], timebuf[MAX_LEN];
	int sd, nsd, nfds;
	int len, clen;
	int i, j;
	int cli_cnt = 0;
	int client_s[MAX_SOCK];
	fd_set read_fds;
	sprintf(SOCK_NAME, "%s/%s", getcwd(NULL, MAX_LEN), "chat_server");
	atexit(ifexit);	// 서버가 종료될 때 수행될 함수를 등록한다. 

	signal(SIGPIPE, SIG_IGN);   // 닫힌 소켓에 쓰는 경우 전달되는 시그널 무시.
	signal(SIGTSTP, SIG_IGN);   // ctrl + z 무시.
	signal(SIGINT, handler);   // ctrl + c는 handler로 처리하여 남은 소켓 파일을 삭제함. 

	sd = connect_cli(SOCK_NAME); // 부모 서버와 클라이언트를 연결하는 소켓을 생성하고 클라이언트 기다림.
	nfds = sd + 1;
	FD_ZERO(&read_fds);

	printf("The service is running..... \n");

	while (1) {
		if ((cli_cnt - 1) >= 0)
			nfds = client_s[cli_cnt - 1] + 1;
		FD_SET(sd, &read_fds);

		for (i = 0; i < cli_cnt; i++)
			FD_SET(client_s[i], &read_fds);

		if (select(nfds, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0) {
			printf("select error\n");
			return -1;
		}
		if (FD_ISSET(sd, &read_fds)) {   // 클라이언트가 부모 서버에 연결하는 경우
			clen = sizeof(struct sockaddr_un);
			nsd = accept(sd, (struct sockaddr *)&cli, &clen);
			if (nsd != -1) {
				int option;
				// 채팅 클라이언트를 목록에 추가 
				client_s[cli_cnt] = nsd;   // 클라이언트와 연결하기 위한 소켓 디스크립터를 배열에 저장.
				cli_cnt++;   // 연결되어 있는 클라이언트 수 1 증가.
				option = move(nsd);   // 클라이언트를 채팅방으로 이동 시킴. (클라이언트가 부모 서버와 연결될 때에는 방을 이동하거나 생성하는 경우뿐이다. )  

				if (option == 0)// 클라이언트에서 방이 있을 때 방 개설, 이동을 선택할 수 있도록 하기 위해 move 함수에서는 list만 출력하고 리턴한 뒤 다시 move함수를 호출한다.
					move(nsd);
			}
		}
	}
	close(sd);
	return 0;
}

int move(int nsd) {   // main 함수로부터 전달 받아 room_list를 move 함수에서 수정.
	struct room * next_room;
	struct pass pass;
	char * send_msg = (char *)malloc(sizeof(char) * MAX_LEN);
	char buf[MAX_LEN];
	char chatfile[MAX_LEN];
	int pid, len;
	int room_num, option;

	sprintf(buf, "%d", room_cnt);   // 클라이언트에게 전달하기 위해 방 개수를 문자열로 변환
	send(nsd, buf, strlen(buf), 0);   // 클라이언트에게 방 개수 전달.
	len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0'; // 클라이언트로부터 옵션 입력 받음.

	if (strcmp(buf, "room_list_print") == 0) {	// 클라이언트가 서버에 처음 연결됐을 때 방의 이동을 선택한 경우에는 move함수에서 list만 출력한 후 반환하고 클라이언트에서 채팅방을 선택한 후 이동할지 또는 채팅방을 개설할지 선택한 후에 다시 move함수를 호출하여 방을 옮기도록한다.
		room_list_print(nsd);   // 채팅방 목록 전달 함수 실행.
		return 0;	// 반환하는 값이 0인 경우 main함수에서 move 함수를 다시 호출한다.
	}

	option = buf[0] - 48;   // 문자열을 정수로 바꾸어 저장.

	if (option == 2) { // 클라이언트가 방의 생성을 요청한 경우.
		get_room_name(nsd, buf, MAX_LEN, 0);   // 클라이언트로부터 전달 받은 채팅방명을 중복 확인한 후 함수가 반환되면 중복되지 않은 채팅방명이 buf에 저장된다.
		next_room = &room_list[room_cnt]; // 배열의 room_cnt번째 항목에 새로운 방의 정보를 저장.
		strcpy(next_room->room_name, buf); // 클라이언트로부터 전달받은 채팅방 이름으로 초기화
		sprintf(next_room->room_name, "%s", buf); // 방의 채팅 내용을 저장할 파일의 이름 저장.
		sprintf(next_room->sock_name, "%s%d", "sock", room_cnt + 1); // 클라이언트와 연결할 소켓 파일명 저장.
		sprintf(chatfile, "%s_%s.txt", buf, "chat");

		if (mkdir(next_room->room_name, 0766) == -1) { // 채팅방 이름의 폴더 만들기
			perror("mkdir");
			exit(1);
		}
		chdir(next_room->room_name);
		creat("server_test_file", 0644);
		creat(chatfile, 0644);
		chdir("..");
		room_cnt++; // 방 개수 증가.
	}
	else if (option == 3 || option == 8) { // 방을 이동하는 경우
		if (option == 3)	// 클라이언트가 기능을 사용하여 방을 이동하는 경우에는 목록을 출력한다.  
			room_list_print(nsd);	// 하지만 서버에 처음 접속한 경우에는 이전에 목록을 출력한 상태로 move함수를 호출하기 때문에 목록을 출력할 필요가 없다.

		len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0'; // 클라이언트가 이동할 방 번호 입력받음.
		room_num = atoi(buf); // room_num은 사용자가 이동할 방 번호
		next_room = &room_list[room_num - 1]; // 클라이언트가 선택한 채팅방의 번호에 해당하는 구조체를 next_room에 저장.(가독성을 위해서)
	}

	pass.psd = nsd;
	pass.cur_room = next_room;   // 스레드에 전달할 구조체를 초기화한다.

	if (access(next_room->sock_name, F_OK) != -1) {
		// 소켓 파일이 이미 만들어져 있는 경우 해당 채팅방에 이미 사용자가 연결된 상태기 때문에 스레드를 만들 필요 없음.
		send(nsd, next_room->sock_name, strlen(next_room->sock_name), 0);   // 클라이언트에게 소켓 파일명 전송.
	}
	else {   // 채팅방에 연결된 클라이언트가 없는 경우
		pthread_create(&room_list->thread, NULL, chat_process, (void *)&pass);   // 쓰레드를 생성하여 각 채팅방의 서비스를 처리한다.
	}
	return 1;	//  반환값이 1인 경우에는 반환한 후 다시 move함수를 호출하지 않는다.
}

void * chat_process(void * p) {
	struct pass * pass = (struct pass *)p;
	struct sockaddr_un cli;
	struct room * cur_room = pass->cur_room;   // 현재 채팅방의 정보를 담은 구조체(쓰레드가 서비스를 수행함에 따라 정보를 수정하여 main 스레드에서 사용할 수 있도록 한다.)
	struct tm *tm;
	int psd = pass->psd;
	int sd, nsd, nfds;
	int len, clen;
	int i, j;
	int * cli_cnt = &cur_room->cli_cnt;
	int * client_s = cur_room->client_s;
	char buf[MAX_LEN], sender[MAX_LEN];
	char timebuf[MAX_LEN];
	char chatfile[MAX_LEN];
	fd_set read_fds;
	time_t tt;
	FILE *fp;

	*cli_cnt = 0;	// 연결된 클라이언트 수 초기화
	sd = connect_cli(cur_room->sock_name);   // 구조체에 저장된 소켓 파일명으로 소켓을 생성하고 클라이언트를 기다린다.
	send(psd, cur_room->sock_name, strlen(cur_room->sock_name), 0);   // 클라이언트에게 소켓 파일명 전송.(클라이언트가 해당 소켓에 연결할 수 있도록 전송함.)
	nfds = sd + 1;
	FD_ZERO(&read_fds);

	while (1) {
		if ((*cli_cnt - 1) >= 0)
			nfds = client_s[*cli_cnt - 1] + 1;

		FD_SET(sd, &read_fds);

		for (i = 0; i < *cli_cnt; i++)
			FD_SET(client_s[i], &read_fds);

		if (select(nfds, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0) {
			printf("select error\n");
			exit(1);
		}

		if (FD_ISSET(sd, &read_fds)) {
			clen = sizeof(cli);
			nsd = accept(sd, (struct sockaddr *)&cli, &clen);
			if (nsd != -1) {
				len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0';// 클라이언트로부터 유저 이름 전달 받음.
				cur_room->user_list[*cli_cnt] = (char *)malloc(sizeof(char) * MAX_LEN);
				strcpy(cur_room->user_list[*cli_cnt], buf);   // 유저 이름을 배열에 저장하여 관리.
				send(nsd, cur_room->room_name, strlen(cur_room->room_name), 0);   // 연결된 클라이언트에게 채팅방 이름 전달(화면에 채팅방 이름을 출력하는데 사용된다.)

				len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0';// 클라이언트로부터 arg 전달받음.//***** 추가
				cur_room->user_arg[*cli_cnt] = atoi(buf); //***** 추가
				// 클라이언트를 목록에 추가
				client_s[*cli_cnt] = nsd;   // 연결된 클라이언트의 소켓 디스크립터를 배열에 저장하여 관리.

				(*cli_cnt)++;   // 연결된 클라이언트의 수를 1 증가시킨다.

				for (i = 0; i < *cli_cnt; i++) {   // 채팅방 참여자들 모두에게 입장 메시지를 전달한다.
					if (cur_room->user_arg[i] == 0) //****** 추가
						sprintf(sender, "\t\t**** %s님이 입장하셨습니다 ****", cur_room->user_list[*cli_cnt - 1]);   // 유저 이름과 입장 메시지를 연결하여 전송
					else if (cur_room->user_arg[i] == 1)
						sprintf(sender, "\t\t**** %s has joined ****", cur_room->user_list[*cli_cnt - 1]);   // 유저 이름과 입장 메시지를 연결하여 전송
					send(client_s[i], sender, strlen(sender), 0);
				}
			}
		}
		for (i = 0; i < *cli_cnt; i++) {
			if (FD_ISSET(client_s[i], &read_fds)) {
				if ((len = recv(client_s[i], buf, MAX_LEN, 0)) > 0) {	// 클라이언트로부터 메시지를 전달받는 경우
					buf[len] = '\0';	// 클라이언트가 NULL을 제외한 문자열을 전달하기 때문에 끝에 NULL을 삽입한다.
					if (strcmp(buf, "room_list_print") == 0) {   // 사용자가 채팅방 목록의 출력을 원하는 경우 목록을 전달해줌.
						sprintf(sender, "%d", room_cnt);   // 채팅방의 개수를 클라이언트에게 전달하기 위해 문자열로 변환
						send(client_s[i], sender, strlen(sender), 0);   // 방의 개수 전달.
						room_list_print(client_s[i]);   // 채팅방 목록 전달 함수 실행.
					}
					else if (strcmp(buf, "clear") == 0) {
						send(client_s[i], cur_room->room_name, strlen(cur_room->room_name), 0);   // 채팅방 이름 전달, 클라이언트에서 채팅방을 clear한 후 화면에 입장 메시지를 출력할 때 사용된다.
					}
					else {
						if (strstr(buf, "exit") != NULL) {         //  종료문자 입력시 채팅 탈퇴 처리
							shutdown(client_s[i], 2);   // 채팅방을 나간 클라이언트와 연결하는 소켓 닫음.
							if (i != *cli_cnt - 1) {   // 가장 마지막에 연결된 클라이언트가 아니라면
								client_s[i] = client_s[*cli_cnt - 1];   // 닫은 소켓의 인덱스에 마지막 인덱스의 소켓을 복사.
								strcpy(cur_room->user_list[i], cur_room->user_list[*cli_cnt - 1]); // 유저명을 저장한 배열도 똑같이 동작.
								cur_room->user_arg[i] = cur_room->user_arg[*cli_cnt - 1];
							}
							(*cli_cnt)--;   // 연결된 클라이언트의 수를 1 감소 시킨다.
							if (*cli_cnt == 0) {
								// 연결된 클라이언트가 더이상 없으면 소켓 파일 삭제(클라이언트는 SIGTSTP, SIGINT로는 
								// 종료할 수 없으며, SIGQUIT으로 종료해도 서버로 종료메시지를 전송하도록 했음.
								remove(cur_room->sock_name);   // 해당 소켓 파일을 삭제.
							}
							for (int j = 0; j < *cli_cnt; j++) {   // 채팅방 참여자들 모두에게 퇴장 메시지를 전달한다.
								if (j != i) {
									if (cur_room->user_arg[j] == 0)
										sprintf(sender, "\t\t**** %s님이 나갔습니다 ****", cur_room->user_list[i]);  // 유저 이름과 입장 메시지를 연결하여 전송
									else if (cur_room->user_arg[j] == 1)
										sprintf(sender, "\t\t**** %s left the room ****", cur_room->user_list[i]);  // 유저 이름과 입장 메시지를 연결하여 전송

									send(client_s[j], sender, strlen(sender), 0);
								}
							}
						}
						else {	// 모든 채팅 참가자에게 메시지 전달
							time(&tt);
							tm = localtime(&tt);
							strftime(timebuf, sizeof(timebuf), output[0], tm);

							sprintf(chatfile, "%s_%s.txt", cur_room->room_name, "chat");
							chdir(cur_room->room_name);
							if ((fp = fopen(chatfile, "a")) == NULL) {
								perror("fopen");
								exit(1);
							}
							fputs(timebuf, fp);
							fputs(buf, fp);
							fputs("\n", fp);
							fclose(fp);
							chdir("..");

							for (j = 0; j < *cli_cnt; j++) {
								if (j != i) {
									sprintf(sender, "\t\t\t\t\t%s%s", timebuf, buf);
									send(client_s[j], sender, strlen(sender), 0);
								}
								else {
									sprintf(sender, "%s %s", buf, timebuf);
									send(client_s[j], sender, strlen(sender), 0);
								}
							}
						}
					}
				}
			}
		}
	}
}

void ifexit() {
	struct dirent *dent;
	DIR *dp;
	int i, j;

	if (room_cnt != 0) {
		for (i = 0; i < room_cnt; i++) {   // 소켓 파일을 모두 삭제함.
			remove(room_list[i].sock_name);
		}
		remove(SOCK_NAME);   // 클라이언트와 부모 서버를 연결하는 socket 삭제.

		for (j = 0; j < room_cnt; j++) {
			if ((dp = opendir(room_list[j].room_name)) == NULL) { //현재 dir open
				perror("opendir: ./");
				exit(1);
			}
			chdir(room_list[j].room_name);
			while ((dent = readdir(dp)) != NULL) {
				remove(dent->d_name); // 폴더 내 파일들도 제거 
			}
			chdir("..");
			rmdir(room_list[j].room_name); // 디렉토리 제거 
		}
		for (int i = 0; i < room_cnt; i++) {   // 서버가 종료되면 모든 클라이언트에게 서버가 종료되었음을 알리고 클라이언트도 종료시킨다.
			for (int j = 0; j < room_list[i].cli_cnt; j++) {
				send(room_list[i].client_s[j], "exit", strlen("exit"), 0);
				close(room_list[i].client_s[j]);
			}
		}
		remove(SOCK_NAME);   // 부모 서버와 클라이언트를 연결하는 소켓 파일도 삭제.
	 //==========12/6 추가========//
		closedir(dp);
		//==========================//
	}
	else {
		remove(SOCK_NAME);
	}
	printf("\nShut down the server. \n");
}

void room_list_print(int psd) {
	int i, j;
	char buf[MAX_LEN];

	for (i = 0; i < room_cnt; i++) {   // 클라이언트에게 어떤 채팅방이 있는지 알려줌.
		sprintf(buf, "%d. %s (%d)", i + 1, room_list[i].room_name, room_list[i].cli_cnt);	// 채팅방 번호, 채팅방 이름, 참여자 수를 이어붙인다.
		for (j = 0; j < room_list[i].cli_cnt; j++) {	// 해당 채팅방의 클라이언트 수 만큼 실행하면서 참여자의 닉네임을 문자열에 붙여준다.
			if (j == 0)
				strcat(buf, "[");
			strcat(buf, room_list[i].user_list[j]);
			if (j == room_list[i].cli_cnt - 1)
				strcat(buf, "]");
			else
				strcat(buf, ", ");
		}
		send(psd, buf, strlen(buf), 0);	// 처리가 끝난 문자열을 클라이언트에 전송한다.
	}
}

void get_room_name(int sockfd, char buf[], size_t len, int flags) {   // 사용자로부터 전달받은 채팅방 이름이 이미 존재하는지 확인하여, 그 결과를 클라이언트에게 알린다. 
																	  // 중복된 경우 클라이언트는 사용자로부터 채팅방 이름을 다시 입력받고, 중복되지 않은 경우 채팅방을 해당 이름으로 개설한다.
	int flag = 0;
	int length = len;

	do {
		len = recv(sockfd, buf, length, flags);   buf[len] = '\0'; // 클라이언트로부터 채팅방 이름 전달 받음.
		flag = 0;   // 중복 여부를 나타내는 플래그
		for (int i = 0; i < room_cnt; i++) {   // 채팅방 개수만큼 확인.
			if (strcmp(buf, room_list[i].room_name) == 0) {   // 같은 이름의 채팅방이 존재한다면.
				flag = 1;   // 플래그를 변경하여 반복되도록 함.
				strcpy(buf, "fail");
				send(sockfd, buf, strlen(buf), 0);   // fail 메시지 전달, 클라이언트는 이 메시지를 전달받아 중복 여부를 확인하고 그에 따라 사용자에게 다시 채팅방 이름을 입력받는다.
				break;
			}
		}
	} while (flag == 1);

	send(sockfd, buf, strlen(buf), 0);   // 중복되지 않은 채팅방 이름이라면 클라이언트가 전달한 문자열을 그대로 전달함으로써 중복되지 않은 채팅방 이름임을 알린다.
	len = recv(sockfd, buf, length, flags);   buf[len] = '\0';// 서버는 채팅방 이름을 다시 전달 받아 클라이언트가 수신했음을 확인한다.
															  // (이 동작이 추가되지 않으면 위의 send 코드에서 sock_name까지 전달되는 오류를 막기 위하여 추가한 동작이다.)
}

int connect_cli(char * sock_name) {   //   소켓 파일명을 sock_name으로하여 소켓을 생성하고 클라이언트를 기다림.
	struct sockaddr_un ser, cli;
	int sd, len, clen;

	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {   // 소켓 생성
		perror("socket");
		exit(1);
	}

	memset((char *)&ser, 0, sizeof(struct sockaddr_un));
	ser.sun_family = AF_UNIX;
	strcpy(ser.sun_path, sock_name);
	len = sizeof(ser.sun_family) + strlen(ser.sun_path);

	if (bind(sd, (struct sockaddr *)&ser, len)) {
		perror("bind");
		exit(1);
	}

	if (listen(sd, MAX_USR) < 0) {
		perror("listen");
		exit(1);
	}
	return sd;
}

void handler(int signo) {   // ctrl + c 입력하여 서버를 종료시키는 경우
	exit(1);   // 종료시켜 atexit 함수를 통해 등록시킨 ifexit 함수가 동작하도록 한다.
}