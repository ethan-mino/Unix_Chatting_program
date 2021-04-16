#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#define MAX_LEN 256
#define SOCK_NAME "chat_server"
#define EXIT_MESSAGE "exit"


void tstp_handler(int signo);
void quit_handler(int signo);
void int_handler(int signo);
void move(int room_num, int option);
void create();
int connect_cli(char * sock_name);
void room_list_print(int room_num);
void get_room_name(int sockfd, char buf[], size_t len, int flags);
void ifexit(void); // atexit의 핸들러 함수 
void lsls(char* dirname);
void cpcp(char *dirname, char *argv1, char *argv2);
void make_dirname(char* testfile); // 중복을 확인한 후 디렉토리 만드는 함수

int sd, arg = 0; //********** 추가
char name[50];   // 사용자 id  
char serv_dirname[50];
char dirname[255];

int main(int argc, char * argv[]) {	//******************
	struct sockaddr_un ser;
	struct dirent *dent;
	char buf[MAX_LEN];
	char * send_str;
	char testfile[50];
	int len, pid, nlen;
	int room_num;   // 서버에 있는 방의 개수 저장하는 변수.

	fd_set read_fds;
	DIR *dp;

	if (argc != 1) {	// 옵션이 하나 이상 있는 경우 //********** if문 추가
		++argv;	// 프로그램명을 제외한 첫번째 인자를 확인하여 사용자가 전달한 옵션이 유효한지 확인.
		if (strcmp(++(*argv), "a") != 0 || argc > 2) {	// a옵션이 아니거나 인자가 여러개 지정된 경우 에러 출력
			printf("client: invalid option\n");
			exit(1);
		}
		arg = 1;
	}

	atexit(ifexit);    // atexit 함수 등록

	signal(SIGTSTP, tstp_handler);   // ctrl + z로 종료할 수 없음. 종료 안내메시지 출력.
	signal(SIGQUIT, quit_handler);   // ctrl + ￦를 입력하면 서버에게 EXIT_MESSAGE를 전달하여 연결되어 있는 클라이언트의 개수를 하나 줄이도록 하고, exit될 때 호출되는 함수를 실행하여 클라이언트가 종료될 때 처리해야할 작업을 수행한다.

	if (arg == 0) //********** if else문 추가
		printf("닉네임을 입력해주세요! : ");
	else if (arg == 1)
		printf("Please enter a nickname! : ");

	scanf("%s", name); getchar();   // 사용자 아이디 입력받음.
	sd = connect_cli(SOCK_NAME);   // "chat_server"를 소켓 파일의 이름으로하여 서버와 연결

	sprintf(testfile, "%s%s", "testfile_", name);
	sprintf(dirname, "%s%s", name, "_download");  //
	if ((dp = opendir("./")) == NULL) { //현재 dir open
		perror("opendir: ./");
		exit(1);
	}
	make_dirname(testfile);
	len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // 서버로부터 방 개수 전달받음.
	room_num = atoi(buf);   // 문자열로 된 방 개수를 정수로 바꿔서 변수에 저장.

	if (room_num == 0) {   // 처음 접속했을 때 방이 없는 경우.
		send(sd, "2", strlen("2"), 0); // 서버에게 2번 기능임을 알림.
		create();   // 방을 개설하고 해당 방으로 이동.
	}
	else {   // 방이 있는 경우
		char option[10];
		send(sd, "room_list_print", strlen("room_list_print"), 0);	// 서버에게 	"room_list_print"를 전달하여 채팅방 목록을 전달해줄 것을 요청
		room_list_print(room_num);	// 채팅방 목록을 출력한다.

		while (1) {	// 방을 개설할지, 이동할지를 입력받고, 1번 또는 2번을 입력할 때까지 반복하여 입력 받음.
			if (arg == 0) { //********** if else문 추가
				printf("\n1. 방 개설 2. 이동 \n");
				printf(">> 선택 : ");
			}
			else if (arg == 1) {
				printf("\n1. Create a room 2. Move to a room \n");
				printf(">> Select : ");
			}

			fgets(option, MAX_LEN, stdin);	option[strlen(option) - 1] = '\0';
			if ((strcmp(option, "1") == 0 || strcmp(option, "2") == 0)) {
				break;
			}
			else {	//******* 추가
				if (arg == 0)
					printf("1번 또는 2번을 선택해주세요. \n");
				else if (arg == 1) {
					printf("Please select 1 or 2. \n");
				}
			}
			printf("\n\n");
		}

		if (strcmp(option, "1") == 0) {	// 방을 개설하는 경우
			len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // 서버로부터 방 개수 전달받음.
			send(sd, "2", strlen("2"), 0); // 서버에게 2번 기능임을 알림.
			create();   // 방을 개설하고 해당 방으로 이동.
		}
		else if (strcmp(option, "2") == 0)   // 이동할 방을 선택하고 해당 방으로 이동.
		{
			len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // 서버로부터 방 개수 전달받음.
			move(room_num, 8);	// 옵션을 8로 주어 서버에 처음 연결되었을 때 방을 이동하는 동작을 수행한다.
		}
	}
	signal(SIGINT, int_handler);    // ctrl + c하면 기능을 선택할 수 있게 int_handler 호츌.  
									// 자식 서버에 연결된 후에 시그널 핸들러를 등록하여 채팅방 들어가기 전에 사용할 수 없도록 했음.
	FD_ZERO(&read_fds);

	while (1) {
		FD_SET(0, &read_fds);
		FD_SET(sd, &read_fds);
		if (select(sd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
			// 시그널 받아서 block되어 있던 select에서 에러가 발생하면 다시 select
		{
			select(sd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
		}

		if (FD_ISSET(sd, &read_fds)) {   // 서버로부터 문자열이 전달되는 경우
			int size;
			if ((size = recv(sd, buf, MAX_LEN, 0)) > 0) {
				buf[size] = '\0';   // 서버가 끝에 NULL을 넣어 전달하지 않으므로 맨 끝에 NULL 삽입.
				if (strcmp(buf, "exit") == 0)
					// 서버가 종료될 때 exit 메시지를 전송하는데, 이 메시지를 전달받은 경우 클라이언트도 같이 종료시켜 소켓 파일도 같이 삭제.
				{
					exit(1);
				}
				printf("%s \n", buf);   // 전달 받은 문자열 출력.
			}
		}

		if (FD_ISSET(0, &read_fds)) {   // 표준입력으로부터 문자열 입력받는 경우.
			send_str = (char *)malloc(MAX_LEN);   // 서버로 전달될 문자열을 저장할 변수.
			if (fgets(send_str, MAX_LEN, stdin) > 0) {   // 표준입력으로부터 문자열 입력 받음.
				send_str[strlen(send_str) - 1] = '\0';	// 개행을 삭제한다.(클라이언트와 서버는 각각 문자열에서 개행을 제거한 후 전달하고, 전달 받을 때는 뒤에 NULL을 붙여 사용한다.)
				int size = strlen(send_str);

				if (strcmp(send_str, "clear") == 0) {	// 사용자가 화면의 내용을 지우길 원하는 경우
					send(sd, "clear", strlen("clear"), 0);   // 서버에 "clear" 전달하여 clear 명령을 수행할 것을 알림.
					len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // 서버로부터 채팅방 이름 전달 받음.
					system("clear");   // clear 명령 수행

					if (arg == 0)	//******* 추가
						printf("\t\t**** %s 방에 입장하셨습니다 **** \n", buf);      // 채팅방 이름 출력.
					else if (arg == 1)
						printf("\t\t**** Entered %s room **** \n", buf);
				}
				else if (strcmp(send_str, EXIT_MESSAGE) == 0) {   // 사용자가 종료 메시지를 입력한 경우
					if (arg == 0) //************ 추가
						printf("종료하려면 ctrl + ￦를 눌러주세요\n");
					else if (arg == 1)
						printf("Press ctrl + ￦ to exit \n");
				}
				else {
					nlen = strlen(name);
					sprintf(buf, "[%s] : %s", name, send_str);   // 서버에 보낼 문자열에 사용자의 이름을 붙여줌.
					if (send(sd, buf, size + nlen + 5, 0) != (size + nlen + 5))   // 에러 발생한 경우
						printf("Error : error on socket.\n");   // 에러 메시지 출력.            
				}
			}
		}
	}
	close(sd);
	closedir(dp);
	return 0;
}

void get_room_name(int sockfd, char buf[], size_t len, int flags) {   // 중복된 채팅방 이름인지 확인하기 위한 함수
	int flag = 0, length = len;
	char answer[MAX_LEN];

	do {
		flag = 0;
		fgets(buf, length, stdin); buf[strlen(buf) - 1] = '\0';  // 개설할 방 이름 입력 받음.
		send(sockfd, buf, strlen(buf), flags);   // 사용자가 입력한 방 이름을 서버에 전달한다.
		len = recv(sockfd, answer, length, flags);   answer[len] = '\0';  // 서버로부터 채팅방 이름이 유효한지 
															//전달받음 유효하면 클라이언트가 전달한 이름을 그대로, 실패하면 fail을 전송한다.
		if (strcmp(buf, answer) != 0) {   // 채팅방 이름이 중복된 경우
			if (arg == 0) {	//******* 추가
				printf("\n채팅방 이름이 이미 존재합니다. \n");
				printf("재입력 >> ");
			}
			else if (arg == 1) {
				printf("\nThe chat room name already exists. \n");
				printf("Re-enter >> ");
			}


			flag = 1;
		}
	} while (flag == 1);
	send(sockfd, buf, strlen(buf), 0);	// 채팅방명을 다시 서버에 전송한다.
}

// 방을 생성하고 클라이언트를 생성한 방으로 연결해주는 함수. 
// (부모와의 연결을 끊고, 자식 서버와 연결해줌, 부모는 클라이언트의 방 생성/이동을 관리하고, 자식 서버는 채팅방 서비스를 처리한다.)

void create() {
	char buf[MAX_LEN], room_name[MAX_LEN];
	int len;
	if (arg == 0)	//******** 추가
		printf("개설할 방 이름 : ");
	else if (arg == 1)
		printf("Room name to create : ");

	get_room_name(sd, buf, MAX_LEN, 0);   // 사용자가 중복되지 않은 방 이름을 입력할 때까지 
										  // 서버에 전달하여 유효성을 검사함.-> 방 목록은 부모 서버가 관리
	len = recv(sd, buf, MAX_LEN, 0);   buf[len] = '\0';// 부모 서버로부터 소켓 파일 이름 전달 받음.
	close(sd);   // 부모 서버와 연결 끊음.
	sd = connect_cli(buf);   // 부모 서버로부터 전달받은 소켓 파일명으로 자식 서버와 연결.
	send(sd, name, strlen(name), 0);   // 사용자명을 서버에 전달한다.
	len = recv(sd, room_name, MAX_LEN, 0);   room_name[len] = '\0';   // 방 이름 전달 받음.
	strcpy(serv_dirname, room_name);    //받은 방 이름을 파일 업/다운로드에 사용하도록 serv_dirname에 저장함
	sprintf(buf, "%d", arg);	// ****** 추가
	send(sd, buf, strlen(buf), 0); // ****** 추가
	system("clear");   // 채팅화면 clear


	if (arg == 0)	//******* 추가
		printf("\t\t**** %s 방에 입장하셨습니다 ****\n", room_name);      // 채팅방 이름 출력.
	else if (arg == 1)
		printf("\t\t**** Entered %s room **** \n", room_name);
}

void room_list_print(int room_num) { //***********추가
	int len;
	char buf[MAX_LEN];

	if (arg == 0)	//***** 추가
		printf("\n*************** 채팅방 목록 ***************\n\n");
	else if (arg == 1)
		printf("\n*************** Chat room list ***************\n\n");
	for (int i = 0; i < room_num; i++) {   // 방의 개수만큼 서버로부터 채팅방에 대한 정보를 전달 받아서 출력.
		if ((len = recv(sd, buf, MAX_LEN, 0)) > 0) {
			buf[len] = '\0';
			printf("%s \n", buf);
			if (i != room_num - 1)
				printf("\n");
		}
	}
	printf("\n******************************************\n");
}

void move(int room_num, int option) {   // 이미 만들어진 방으로 이동한다..
	int len;
	char buf[MAX_LEN], room_name[MAX_LEN];
	sprintf(buf, "%d", option);
	send(sd, buf, strlen(buf), 0); // 서버에게 3번 또는 8번 기능임을 알림.
	// 만약 채팅 방이 개설되어 있는 상태에서 사용자가 입장했을 때 방 이동을 선택한 경우 
	// move함수에 3을 전달하여 메뉴를 출력하지않도록 한다.(사용자가 처음 서버에 접속한 경우에은 move함수를 호출하기 전에 이미 list를 출력하기 때문에)
	if (option == 3) {
		if (arg == 0) { //추가
			room_list_print(room_num);   // 방 목록 출력, 만들어진 방이 없는 경우 함수 호출 전 처리해줬음.
			printf("\n**** 이동할 방 번호를 선택해주세요. ****\n\n");
			printf(">> 선택 : ");
		}
		else if (arg == 1) {
			room_list_print(room_num);   // 방 목록 출력, 만들어진 방이 없는 경우 함수 호출 전 처리해줬음.
			printf("\n**** Please select a room number to move. ****\n\n");
			printf(">> Select : ");
		}
	}
	else if (option == 8) {
		if (arg == 0) //추가
			printf(">> 이동할 방 번호 선택 : ");
		else if (arg == 1)
			printf("Select room number to move : ");
	}
	scanf("%s", buf); getchar();// 사용자로부터 방 번호 입력 받음.
	while ((atoi(buf) <= 0 || atoi(buf) > room_num)) {    // 사용자가 방 번호를 입력하지 않으면 오류 방지를 위해 계속해서 입력받는다.
		if (arg == 0)
			printf("생성되어 있는 방 번호(0 ~ %d)를 입력해주세요! \n", room_num);
		else if (arg == 1)
			printf("Please enter the room number(0 ~ %d) created! \n", room_num);

		scanf("%s", buf); getchar();// 사용자로부터 방 번호 입력 받음.
	}
	send(sd, buf, strlen(buf), 0);   // 부모 서버에 방 번호 전달.
	len = recv(sd, buf, MAX_LEN, 0);   buf[len] = '\0';   // 부모 서버로부터 소켓 파일 이름 전달 받음.
	close(sd);   // 부모 서버와의 연결 끊음.
	sd = connect_cli(buf);   // 자식 서버와 연결
	send(sd, name, strlen(name), 0);   // 사용자명을 서버에 전달한다.
	len = recv(sd, room_name, MAX_LEN, 0);   room_name[len] = '\0';   // 방 이름 전달 받음.
	strcpy(serv_dirname, room_name);
	sprintf(buf, "%d", arg);	// ****** 추가
	send(sd, buf, strlen(buf), 0); // ****** 추가

	system("clear");
	if (arg == 0)	//******* 추가
		printf("\t\t**** %s 방에 입장하셨습니다 **** \n", room_name);      // 채팅방 이름 출력.
	else if (arg == 1)
		printf("\t\t**** Entered %s room **** \n", room_name);
}

void int_handler(int signo) {   // SIGINT 시그널에 대한 handler
	struct dirent *dent;
	char buf[MAX_LEN];
	char filename[50];
	int search_flag = 0;
	int opt;
	int option, room_num, len;
	char chatfile[MAX_LEN];
	char tmpbuf[MAX_LEN];
	char searchbuf[MAX_LEN];
	FILE *fp;
	DIR *dp;

	system("clear");
	if (arg == 0) {
		printf("1. 채팅방 목록 확인 2. 채팅방 생성 3. 채팅방 이동 4. 파일 올리기 \n5. 파일 내려받기 6, 내 파일목록 7. 채팅 검색 \n");   // 기능 출력.
		printf("\n>> 선택 : ");
	}
	else if (arg == 1) {
		printf("1. Check chat room list 2. Create chat room 3. Move chat room 4. Upload file \n5. Download file 6, My file list 7. Chat search \n");
		printf("\n>> Select : ");
	}

	scanf("%s", buf);   getchar();   // 어떤 기능을 사용할지 사용자로부터 입력받음.
	option = atoi(buf);
	if (option < 1 || option > 7) {
		printf(">> 입력한 기능이 없습니다. 채팅방으로 돌아갑니다.. \n\n");
		return;
	}

	if (option == 2 || option == 3) { // 방을 생성하거나 이동하는 경우 처리
		send(sd, EXIT_MESSAGE, strlen(EXIT_MESSAGE), 0);
		// 자식 서버와 연결을 끊기전에 exit 메시지를 전달하여 클라이언트가 연결 해제했음을 알림.
		close(sd);   // 방을 이동할 것이기 때문에 자식 서버와의 연결 끊음.
		sd = connect_cli(SOCK_NAME);   // 방의 이동/생성을 관리하는 부모 서버와 연결
		len = recv(sd, buf, MAX_LEN, 0);   buf[len] = '\0';// 부모 서버로부터 방 개수 전달받음.
		room_num = atoi(buf);   // 문자열로된 방 개수를 정수로 변환하여 저장.
		sprintf(buf, "%d", option);   // 서버로 전달하기 위해 사용자로부터 입력받은 option를 문자열로 변환.
		send(sd, buf, strlen(buf), 0);   // 서버에 옵션 전달.
	}

	if (option == 1) {	// 채팅방 목록 출력
		send(sd, "room_list_print", strlen("room_list_print"), 0);	// 서버에게 	"room_list_print"를 전달하여 채팅방 목록을 전달해줄 것을 요청 
		len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0'; // 채팅방 개수 전달 받음.
		room_num = atoi(buf);
		room_list_print(room_num);	// 방 개수만큼 채팅방 정보 출력.
	}

	else if (option == 2) {   // 방 개설.   
		create();   // 방 생성하고 해당 방으로 이동.
	}

	else if (option == 3) {   // 방 이동
		move(room_num, 3);   // 이동할 방을 선택하고 해당 방으로 이동.
	}

	else if (option == 4) { // 파일 업로드
		lsls(dirname);  // 내 폴더의 파일 목록 보여주기 
		if (arg == 0)
			printf("업로드 할 파일 명 : ");
		else if (arg == 1)
			printf("File name to upload : ");

		scanf("%s", filename); getchar();// 업로드할 파일 명을 입력받아 filename에 저장함 
		if ((dp = opendir(dirname)) == NULL) { //내 폴더 오픈 
			perror("opendir: ./");
			exit(1);
		}
		while ((dent = readdir(dp)) != NULL) { //폴더 안 파일들을 하나씩 추적
			if (strcmp(filename, dent->d_name) == 0) { // 업로드할 파일이 제대로 있다면 
				search_flag = 1; // flag == 1

				if (arg == 0) {
					printf("%s(을)를 전송합니다.\n", filename);
					cpcp(dirname, filename, serv_dirname); // 서버 폴더로 cp 
					printf("%s 전송 완료.\n\n", filename);
				}
				else if (arg == 1) {
					printf("Send %s.\n", filename);
					cpcp(dirname, filename, serv_dirname); // 서버 폴더로 cp 
					printf("%s Transfer complete. \n\n", filename);
				}

			}
		}
		if (search_flag != 1) { // 업로드할 파일이 없다면 
			if (arg == 0)
				printf("\n>> %s 파일이 없습니다. \n\n", filename);
			else if (arg == 1)
				printf("\n>> The %s file does not exist. \n\n", filename);
			search_flag = 0;
		}
		closedir(dp);
	}
	else if (option == 5) { // 파일 다운로드 
		lsls(serv_dirname); // 서버의 폴더 목록 보여주기 
		if (arg == 0)
			printf("다운로드 할 파일 명 : ");
		else if (arg == 1)
			printf("File name to download : ");

		scanf("%s", filename); getchar();// 다운로드할 파일 명을 입력받아 filename에 저장 
		if ((dp = opendir(serv_dirname)) == NULL) { //서버의  폴더 open
			perror("opendir: ./");
			exit(1);
		}
		while ((dent = readdir(dp)) != NULL) {
			if (strcmp(filename, dent->d_name) == 0) { // 파일이 제대로 있다면 
				search_flag = 1;
				if (arg == 0) {
					printf("%s(을)를 다운로드 합니다.\n", filename);
					cpcp(serv_dirname, filename, dirname); // 서버폴더에서 내 폴더로 cp
					printf("%s 다운로드 완료. \n\n", filename);
				}
				else if (arg == 1) {
					printf("Download %s.\n", filename);
					cpcp(serv_dirname, filename, dirname); // 서버폴더에서 내 폴더로 cp
					printf("%s Download complete.. \n\n", filename);
				}
			}
		}
		if (search_flag != 1) {
			if (arg == 0)
				printf("\n>> %s 파일이 없습니다. \n\n", filename);
			else if (arg == 1)
				printf("\n>> The %s file does not exist. \n\n", filename);
			search_flag = 0;
		}
		closedir(dp);
	}

	else if (option == 6) {
		lsls(dirname); // 내 폴더 목록 보여주기
	}

	else if (option == 7) {
		sprintf(chatfile, "%s_%s.txt", serv_dirname, "chat");
		chdir(serv_dirname);
		if ((fp = fopen(chatfile, "r")) == NULL) {
			perror("fopen");
			exit(1);
		}
		printf("\n\n===============================================\n");
		if (arg == 0) {
			printf("[옵션을 선택해주세요]\n");
			printf(">> 1.시간 검색 2.단어 검색\n");
		}
		else if (arg == 1) {
			printf("[Please choose an option]\n");
			printf(">> 1.Time Search 2. Word Search\n");
		}

		printf(">> 선택 : ");
		scanf("%d", &opt);	getchar();
		if (opt == 1) {
			if (arg == 0)
				printf(">> 검색할 시간 입력 (hh:mm) : ");
			else if (arg == 1)
				printf(">> Enter time to search (hh:mm) : ");
			scanf("%s", searchbuf);
			search_flag = 0;
			while (fgets(tmpbuf, sizeof(tmpbuf), fp) != NULL) {
				if (search_flag != 0)
					printf("%s", tmpbuf);
				else {
					if (strstr(tmpbuf, searchbuf) != NULL) {
						printf("%s", tmpbuf);
						search_flag = 1;
					}
				}
			}
		}
		else {
			if (arg == 0)
				printf(">> 검색할 단어 입력 : ");
			else if (arg == 1)
				printf(">> Enter a word to search for (hh:mm) : ");


			scanf("%s", searchbuf);	getchar();
			while (fgets(tmpbuf, sizeof(tmpbuf), fp) != NULL) {
				if (strstr(tmpbuf, searchbuf) != NULL) {
					printf("%s", tmpbuf);
					search_flag = 1;
				}
			}
		}
		if (search_flag == 0)
			if (arg == 0)
				printf(">> 문자열을 찾지 못했습니다.\n");
			else if (arg == 1)
				printf(">> String not found. \n");
		printf("===============================================\n\n\n");
		search_flag = 0;
		chdir("..");
		fclose(fp);
	}
}

int connect_cli(char * sock_name) {   //   소켓 파일명을 sock_name으로하여 서버와 연결해주는 함수.

	struct sockaddr_un ser, cli;
	int sd;
	int len, clen;

	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	memset((char *)&ser, 0, sizeof(struct sockaddr_un));
	ser.sun_family = AF_UNIX;
	strcpy(ser.sun_path, sock_name);
	len = sizeof(ser.sun_family) + strlen(ser.sun_path);

	if (connect(sd, (struct sockaddr *)&ser, len) < 0) {
		perror("bind");
		exit(1);
	}
	return sd;
}


void lsls(char* dirname) {
	struct dirent **namelist;
	int n, i;
	char is_chat[50];
	sprintf(is_chat, "%s%s", serv_dirname, "_chat.txt");

	n = scandir(dirname, &namelist, NULL, alphasort);
	printf("\n");
	if (arg == 0)
		printf("----- %s 폴더의 파일들 -----\n", dirname);
	else if (arg == 1)
		printf("----- files in %s folder -----\n", dirname);

	for (i = 0; i < n; i++) {
		if (namelist[i]->d_name[0] == '.' || strcmp(namelist[i]->d_name, is_chat) == 0) continue;
		//채팅 파일이나 숨김파일(.으로 시작)에 접근 하면 안되므로 숨겨놓음 
		printf("%-1s  ", namelist[i]->d_name);
	}
	printf("\n");
	printf("-----------------------------------------\n");
	printf("\n");
}

void cpcp(char *dirname, char *argv1, char *argv2) {
	int fd1, fd2;
	int rd;
	char buf[10];
	struct stat st;
	struct stat st2;

	chdir(dirname);

	if ((fd1 = open(argv1, O_RDONLY)) == -1) { // fd1 : open argv1
		perror("open");
		exit(0);
	}
	stat(argv1, &st); //st에 argv1에 대한 stat 저장
	stat(argv2, &st2); //st2에 argv2에 대한 stat 저장


	chdir("..");
	chdir(argv2); // 해당 디렉토리로 이동 후 open 

	if ((fd2 = open(argv2, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) == -1) {
		// fd2 : open argv2, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) : 권한 복사를 위함
		perror("open2");
		exit(0);
	}
	rename(argv2, argv1); // 이후 argv1로 이름 변경 (디렉토리 명과 같은 이름으로 생성되는 것 방지) 
	chdir("..");

	while ((rd = read(fd1, buf, 10)) > 0) {
		if (write(fd2, buf, rd) != rd) perror("Write"); // fd1 -> fd2 로 복사 진행
		close(fd1);
		close(fd2);
	}
}


//============새로 추가한 중복 확인 후 폴더 만드는 함수 ====================

void make_dirname(char* testfile) {
	int i = 1;
	char dir_buf[50];
	char file_buf[50];
	char name_buf[50];
	while (1) {
		if (access(dirname, F_OK) == 0) {
			i++;
			sprintf(name_buf, "%s(%d)", name, i);
			sprintf(dir_buf, "%s%s", name_buf, "_download");
			sprintf(file_buf, "%s%s", "testfile_", name_buf);
			strcpy(dirname, dir_buf);
			strcpy(testfile, file_buf);
		}
		else
			break;
	}
	if (i != 1) {
		if (arg == 0)
			printf("이미 있는 닉네임이라서 개인 폴더를 -%s- 로 생성합니다.\n", dirname);
		else if (arg == 1)
			printf("Create a personal folder called -%s- because it already exists.\n", dirname);
	}

	if (mkdir(dirname, 0766) == -1) {
		perror("mkdir");
		exit(1);
	}

	chdir(dirname);
	creat(testfile, 0644);
	chdir("..");
}
//==============================================================

void ifexit(void) {	// 클라이언트가 종료되면서 처리해야할 동작을 수행하는 함수
	struct dirent *dent;
	DIR *dp;
	/*만들어놓은 디렉토리 지우기*/
	if ((dp = opendir(dirname)) == NULL) { //현재 dir open
		perror("opendir: ./");
		exit(1);
	}
	chdir(dirname);
	while ((dent = readdir(dp)) != NULL) {
		remove(dent->d_name); // 폴더 내 파일들도 제거 
	}
	chdir("..");
	rmdir(dirname); // 디렉토리 제거 
	closedir(dp);
}

void tstp_handler(int signo) {
	if (arg == 0)
		printf("\n종료하시려면 \"ctrl + ￦\"를 입력해주세요 \n");      // 시그널을 재정의 했음을 알린다.
	else if (arg == 1)
		printf("\nEnter \"ctrl + ￦\" to exit \n");
}
void quit_handler(int signo) {
	send(sd, EXIT_MESSAGE, strlen(EXIT_MESSAGE), 0);	// 서버에 종료 메시지를 전달하여, 클라이언트가 접속 해제됨을 알림.
	if (arg == 0)
		printf("\n클라이언트를 종료합니다...\n");
	else if (arg == 1)
		printf("\nQuit the client ...\n");

	exit(1);	// 클라이언트가 종료되면서 처리해야할 동작을 수행하는 함수가 호출되고, 클라이언트가 종료된다.
}