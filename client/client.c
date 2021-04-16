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
void ifexit(void); // atexit�� �ڵ鷯 �Լ� 
void lsls(char* dirname);
void cpcp(char *dirname, char *argv1, char *argv2);
void make_dirname(char* testfile); // �ߺ��� Ȯ���� �� ���丮 ����� �Լ�

int sd, arg = 0; //********** �߰�
char name[50];   // ����� id  
char serv_dirname[50];
char dirname[255];

int main(int argc, char * argv[]) {	//******************
	struct sockaddr_un ser;
	struct dirent *dent;
	char buf[MAX_LEN];
	char * send_str;
	char testfile[50];
	int len, pid, nlen;
	int room_num;   // ������ �ִ� ���� ���� �����ϴ� ����.

	fd_set read_fds;
	DIR *dp;

	if (argc != 1) {	// �ɼ��� �ϳ� �̻� �ִ� ��� //********** if�� �߰�
		++argv;	// ���α׷����� ������ ù��° ���ڸ� Ȯ���Ͽ� ����ڰ� ������ �ɼ��� ��ȿ���� Ȯ��.
		if (strcmp(++(*argv), "a") != 0 || argc > 2) {	// a�ɼ��� �ƴϰų� ���ڰ� ������ ������ ��� ���� ���
			printf("client: invalid option\n");
			exit(1);
		}
		arg = 1;
	}

	atexit(ifexit);    // atexit �Լ� ���

	signal(SIGTSTP, tstp_handler);   // ctrl + z�� ������ �� ����. ���� �ȳ��޽��� ���.
	signal(SIGQUIT, quit_handler);   // ctrl + �ܸ� �Է��ϸ� �������� EXIT_MESSAGE�� �����Ͽ� ����Ǿ� �ִ� Ŭ���̾�Ʈ�� ������ �ϳ� ���̵��� �ϰ�, exit�� �� ȣ��Ǵ� �Լ��� �����Ͽ� Ŭ���̾�Ʈ�� ����� �� ó���ؾ��� �۾��� �����Ѵ�.

	if (arg == 0) //********** if else�� �߰�
		printf("�г����� �Է����ּ���! : ");
	else if (arg == 1)
		printf("Please enter a nickname! : ");

	scanf("%s", name); getchar();   // ����� ���̵� �Է¹���.
	sd = connect_cli(SOCK_NAME);   // "chat_server"�� ���� ������ �̸������Ͽ� ������ ����

	sprintf(testfile, "%s%s", "testfile_", name);
	sprintf(dirname, "%s%s", name, "_download");  //
	if ((dp = opendir("./")) == NULL) { //���� dir open
		perror("opendir: ./");
		exit(1);
	}
	make_dirname(testfile);
	len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // �����κ��� �� ���� ���޹���.
	room_num = atoi(buf);   // ���ڿ��� �� �� ������ ������ �ٲ㼭 ������ ����.

	if (room_num == 0) {   // ó�� �������� �� ���� ���� ���.
		send(sd, "2", strlen("2"), 0); // �������� 2�� ������� �˸�.
		create();   // ���� �����ϰ� �ش� ������ �̵�.
	}
	else {   // ���� �ִ� ���
		char option[10];
		send(sd, "room_list_print", strlen("room_list_print"), 0);	// �������� 	"room_list_print"�� �����Ͽ� ä�ù� ����� �������� ���� ��û
		room_list_print(room_num);	// ä�ù� ����� ����Ѵ�.

		while (1) {	// ���� ��������, �̵������� �Է¹ް�, 1�� �Ǵ� 2���� �Է��� ������ �ݺ��Ͽ� �Է� ����.
			if (arg == 0) { //********** if else�� �߰�
				printf("\n1. �� ���� 2. �̵� \n");
				printf(">> ���� : ");
			}
			else if (arg == 1) {
				printf("\n1. Create a room 2. Move to a room \n");
				printf(">> Select : ");
			}

			fgets(option, MAX_LEN, stdin);	option[strlen(option) - 1] = '\0';
			if ((strcmp(option, "1") == 0 || strcmp(option, "2") == 0)) {
				break;
			}
			else {	//******* �߰�
				if (arg == 0)
					printf("1�� �Ǵ� 2���� �������ּ���. \n");
				else if (arg == 1) {
					printf("Please select 1 or 2. \n");
				}
			}
			printf("\n\n");
		}

		if (strcmp(option, "1") == 0) {	// ���� �����ϴ� ���
			len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // �����κ��� �� ���� ���޹���.
			send(sd, "2", strlen("2"), 0); // �������� 2�� ������� �˸�.
			create();   // ���� �����ϰ� �ش� ������ �̵�.
		}
		else if (strcmp(option, "2") == 0)   // �̵��� ���� �����ϰ� �ش� ������ �̵�.
		{
			len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // �����κ��� �� ���� ���޹���.
			move(room_num, 8);	// �ɼ��� 8�� �־� ������ ó�� ����Ǿ��� �� ���� �̵��ϴ� ������ �����Ѵ�.
		}
	}
	signal(SIGINT, int_handler);    // ctrl + c�ϸ� ����� ������ �� �ְ� int_handler ȣ��.  
									// �ڽ� ������ ����� �Ŀ� �ñ׳� �ڵ鷯�� ����Ͽ� ä�ù� ���� ���� ����� �� ������ ����.
	FD_ZERO(&read_fds);

	while (1) {
		FD_SET(0, &read_fds);
		FD_SET(sd, &read_fds);
		if (select(sd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
			// �ñ׳� �޾Ƽ� block�Ǿ� �ִ� select���� ������ �߻��ϸ� �ٽ� select
		{
			select(sd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
		}

		if (FD_ISSET(sd, &read_fds)) {   // �����κ��� ���ڿ��� ���޵Ǵ� ���
			int size;
			if ((size = recv(sd, buf, MAX_LEN, 0)) > 0) {
				buf[size] = '\0';   // ������ ���� NULL�� �־� �������� �����Ƿ� �� ���� NULL ����.
				if (strcmp(buf, "exit") == 0)
					// ������ ����� �� exit �޽����� �����ϴµ�, �� �޽����� ���޹��� ��� Ŭ���̾�Ʈ�� ���� ������� ���� ���ϵ� ���� ����.
				{
					exit(1);
				}
				printf("%s \n", buf);   // ���� ���� ���ڿ� ���.
			}
		}

		if (FD_ISSET(0, &read_fds)) {   // ǥ���Է����κ��� ���ڿ� �Է¹޴� ���.
			send_str = (char *)malloc(MAX_LEN);   // ������ ���޵� ���ڿ��� ������ ����.
			if (fgets(send_str, MAX_LEN, stdin) > 0) {   // ǥ���Է����κ��� ���ڿ� �Է� ����.
				send_str[strlen(send_str) - 1] = '\0';	// ������ �����Ѵ�.(Ŭ���̾�Ʈ�� ������ ���� ���ڿ����� ������ ������ �� �����ϰ�, ���� ���� ���� �ڿ� NULL�� �ٿ� ����Ѵ�.)
				int size = strlen(send_str);

				if (strcmp(send_str, "clear") == 0) {	// ����ڰ� ȭ���� ������ ����� ���ϴ� ���
					send(sd, "clear", strlen("clear"), 0);   // ������ "clear" �����Ͽ� clear ����� ������ ���� �˸�.
					len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // �����κ��� ä�ù� �̸� ���� ����.
					system("clear");   // clear ��� ����

					if (arg == 0)	//******* �߰�
						printf("\t\t**** %s �濡 �����ϼ̽��ϴ� **** \n", buf);      // ä�ù� �̸� ���.
					else if (arg == 1)
						printf("\t\t**** Entered %s room **** \n", buf);
				}
				else if (strcmp(send_str, EXIT_MESSAGE) == 0) {   // ����ڰ� ���� �޽����� �Է��� ���
					if (arg == 0) //************ �߰�
						printf("�����Ϸ��� ctrl + �ܸ� �����ּ���\n");
					else if (arg == 1)
						printf("Press ctrl + �� to exit \n");
				}
				else {
					nlen = strlen(name);
					sprintf(buf, "[%s] : %s", name, send_str);   // ������ ���� ���ڿ��� ������� �̸��� �ٿ���.
					if (send(sd, buf, size + nlen + 5, 0) != (size + nlen + 5))   // ���� �߻��� ���
						printf("Error : error on socket.\n");   // ���� �޽��� ���.            
				}
			}
		}
	}
	close(sd);
	closedir(dp);
	return 0;
}

void get_room_name(int sockfd, char buf[], size_t len, int flags) {   // �ߺ��� ä�ù� �̸����� Ȯ���ϱ� ���� �Լ�
	int flag = 0, length = len;
	char answer[MAX_LEN];

	do {
		flag = 0;
		fgets(buf, length, stdin); buf[strlen(buf) - 1] = '\0';  // ������ �� �̸� �Է� ����.
		send(sockfd, buf, strlen(buf), flags);   // ����ڰ� �Է��� �� �̸��� ������ �����Ѵ�.
		len = recv(sockfd, answer, length, flags);   answer[len] = '\0';  // �����κ��� ä�ù� �̸��� ��ȿ���� 
															//���޹��� ��ȿ�ϸ� Ŭ���̾�Ʈ�� ������ �̸��� �״��, �����ϸ� fail�� �����Ѵ�.
		if (strcmp(buf, answer) != 0) {   // ä�ù� �̸��� �ߺ��� ���
			if (arg == 0) {	//******* �߰�
				printf("\nä�ù� �̸��� �̹� �����մϴ�. \n");
				printf("���Է� >> ");
			}
			else if (arg == 1) {
				printf("\nThe chat room name already exists. \n");
				printf("Re-enter >> ");
			}


			flag = 1;
		}
	} while (flag == 1);
	send(sockfd, buf, strlen(buf), 0);	// ä�ù���� �ٽ� ������ �����Ѵ�.
}

// ���� �����ϰ� Ŭ���̾�Ʈ�� ������ ������ �������ִ� �Լ�. 
// (�θ���� ������ ����, �ڽ� ������ ��������, �θ�� Ŭ���̾�Ʈ�� �� ����/�̵��� �����ϰ�, �ڽ� ������ ä�ù� ���񽺸� ó���Ѵ�.)

void create() {
	char buf[MAX_LEN], room_name[MAX_LEN];
	int len;
	if (arg == 0)	//******** �߰�
		printf("������ �� �̸� : ");
	else if (arg == 1)
		printf("Room name to create : ");

	get_room_name(sd, buf, MAX_LEN, 0);   // ����ڰ� �ߺ����� ���� �� �̸��� �Է��� ������ 
										  // ������ �����Ͽ� ��ȿ���� �˻���.-> �� ����� �θ� ������ ����
	len = recv(sd, buf, MAX_LEN, 0);   buf[len] = '\0';// �θ� �����κ��� ���� ���� �̸� ���� ����.
	close(sd);   // �θ� ������ ���� ����.
	sd = connect_cli(buf);   // �θ� �����κ��� ���޹��� ���� ���ϸ����� �ڽ� ������ ����.
	send(sd, name, strlen(name), 0);   // ����ڸ��� ������ �����Ѵ�.
	len = recv(sd, room_name, MAX_LEN, 0);   room_name[len] = '\0';   // �� �̸� ���� ����.
	strcpy(serv_dirname, room_name);    //���� �� �̸��� ���� ��/�ٿ�ε忡 ����ϵ��� serv_dirname�� ������
	sprintf(buf, "%d", arg);	// ****** �߰�
	send(sd, buf, strlen(buf), 0); // ****** �߰�
	system("clear");   // ä��ȭ�� clear


	if (arg == 0)	//******* �߰�
		printf("\t\t**** %s �濡 �����ϼ̽��ϴ� ****\n", room_name);      // ä�ù� �̸� ���.
	else if (arg == 1)
		printf("\t\t**** Entered %s room **** \n", room_name);
}

void room_list_print(int room_num) { //***********�߰�
	int len;
	char buf[MAX_LEN];

	if (arg == 0)	//***** �߰�
		printf("\n*************** ä�ù� ��� ***************\n\n");
	else if (arg == 1)
		printf("\n*************** Chat room list ***************\n\n");
	for (int i = 0; i < room_num; i++) {   // ���� ������ŭ �����κ��� ä�ù濡 ���� ������ ���� �޾Ƽ� ���.
		if ((len = recv(sd, buf, MAX_LEN, 0)) > 0) {
			buf[len] = '\0';
			printf("%s \n", buf);
			if (i != room_num - 1)
				printf("\n");
		}
	}
	printf("\n******************************************\n");
}

void move(int room_num, int option) {   // �̹� ������� ������ �̵��Ѵ�..
	int len;
	char buf[MAX_LEN], room_name[MAX_LEN];
	sprintf(buf, "%d", option);
	send(sd, buf, strlen(buf), 0); // �������� 3�� �Ǵ� 8�� ������� �˸�.
	// ���� ä�� ���� �����Ǿ� �ִ� ���¿��� ����ڰ� �������� �� �� �̵��� ������ ��� 
	// move�Լ��� 3�� �����Ͽ� �޴��� ��������ʵ��� �Ѵ�.(����ڰ� ó�� ������ ������ ��쿡�� move�Լ��� ȣ���ϱ� ���� �̹� list�� ����ϱ� ������)
	if (option == 3) {
		if (arg == 0) { //�߰�
			room_list_print(room_num);   // �� ��� ���, ������� ���� ���� ��� �Լ� ȣ�� �� ó��������.
			printf("\n**** �̵��� �� ��ȣ�� �������ּ���. ****\n\n");
			printf(">> ���� : ");
		}
		else if (arg == 1) {
			room_list_print(room_num);   // �� ��� ���, ������� ���� ���� ��� �Լ� ȣ�� �� ó��������.
			printf("\n**** Please select a room number to move. ****\n\n");
			printf(">> Select : ");
		}
	}
	else if (option == 8) {
		if (arg == 0) //�߰�
			printf(">> �̵��� �� ��ȣ ���� : ");
		else if (arg == 1)
			printf("Select room number to move : ");
	}
	scanf("%s", buf); getchar();// ����ڷκ��� �� ��ȣ �Է� ����.
	while ((atoi(buf) <= 0 || atoi(buf) > room_num)) {    // ����ڰ� �� ��ȣ�� �Է����� ������ ���� ������ ���� ����ؼ� �Է¹޴´�.
		if (arg == 0)
			printf("�����Ǿ� �ִ� �� ��ȣ(0 ~ %d)�� �Է����ּ���! \n", room_num);
		else if (arg == 1)
			printf("Please enter the room number(0 ~ %d) created! \n", room_num);

		scanf("%s", buf); getchar();// ����ڷκ��� �� ��ȣ �Է� ����.
	}
	send(sd, buf, strlen(buf), 0);   // �θ� ������ �� ��ȣ ����.
	len = recv(sd, buf, MAX_LEN, 0);   buf[len] = '\0';   // �θ� �����κ��� ���� ���� �̸� ���� ����.
	close(sd);   // �θ� �������� ���� ����.
	sd = connect_cli(buf);   // �ڽ� ������ ����
	send(sd, name, strlen(name), 0);   // ����ڸ��� ������ �����Ѵ�.
	len = recv(sd, room_name, MAX_LEN, 0);   room_name[len] = '\0';   // �� �̸� ���� ����.
	strcpy(serv_dirname, room_name);
	sprintf(buf, "%d", arg);	// ****** �߰�
	send(sd, buf, strlen(buf), 0); // ****** �߰�

	system("clear");
	if (arg == 0)	//******* �߰�
		printf("\t\t**** %s �濡 �����ϼ̽��ϴ� **** \n", room_name);      // ä�ù� �̸� ���.
	else if (arg == 1)
		printf("\t\t**** Entered %s room **** \n", room_name);
}

void int_handler(int signo) {   // SIGINT �ñ׳ο� ���� handler
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
		printf("1. ä�ù� ��� Ȯ�� 2. ä�ù� ���� 3. ä�ù� �̵� 4. ���� �ø��� \n5. ���� �����ޱ� 6, �� ���ϸ�� 7. ä�� �˻� \n");   // ��� ���.
		printf("\n>> ���� : ");
	}
	else if (arg == 1) {
		printf("1. Check chat room list 2. Create chat room 3. Move chat room 4. Upload file \n5. Download file 6, My file list 7. Chat search \n");
		printf("\n>> Select : ");
	}

	scanf("%s", buf);   getchar();   // � ����� ������� ����ڷκ��� �Է¹���.
	option = atoi(buf);
	if (option < 1 || option > 7) {
		printf(">> �Է��� ����� �����ϴ�. ä�ù����� ���ư��ϴ�.. \n\n");
		return;
	}

	if (option == 2 || option == 3) { // ���� �����ϰų� �̵��ϴ� ��� ó��
		send(sd, EXIT_MESSAGE, strlen(EXIT_MESSAGE), 0);
		// �ڽ� ������ ������ �������� exit �޽����� �����Ͽ� Ŭ���̾�Ʈ�� ���� ���������� �˸�.
		close(sd);   // ���� �̵��� ���̱� ������ �ڽ� �������� ���� ����.
		sd = connect_cli(SOCK_NAME);   // ���� �̵�/������ �����ϴ� �θ� ������ ����
		len = recv(sd, buf, MAX_LEN, 0);   buf[len] = '\0';// �θ� �����κ��� �� ���� ���޹���.
		room_num = atoi(buf);   // ���ڿ��ε� �� ������ ������ ��ȯ�Ͽ� ����.
		sprintf(buf, "%d", option);   // ������ �����ϱ� ���� ����ڷκ��� �Է¹��� option�� ���ڿ��� ��ȯ.
		send(sd, buf, strlen(buf), 0);   // ������ �ɼ� ����.
	}

	if (option == 1) {	// ä�ù� ��� ���
		send(sd, "room_list_print", strlen("room_list_print"), 0);	// �������� 	"room_list_print"�� �����Ͽ� ä�ù� ����� �������� ���� ��û 
		len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0'; // ä�ù� ���� ���� ����.
		room_num = atoi(buf);
		room_list_print(room_num);	// �� ������ŭ ä�ù� ���� ���.
	}

	else if (option == 2) {   // �� ����.   
		create();   // �� �����ϰ� �ش� ������ �̵�.
	}

	else if (option == 3) {   // �� �̵�
		move(room_num, 3);   // �̵��� ���� �����ϰ� �ش� ������ �̵�.
	}

	else if (option == 4) { // ���� ���ε�
		lsls(dirname);  // �� ������ ���� ��� �����ֱ� 
		if (arg == 0)
			printf("���ε� �� ���� �� : ");
		else if (arg == 1)
			printf("File name to upload : ");

		scanf("%s", filename); getchar();// ���ε��� ���� ���� �Է¹޾� filename�� ������ 
		if ((dp = opendir(dirname)) == NULL) { //�� ���� ���� 
			perror("opendir: ./");
			exit(1);
		}
		while ((dent = readdir(dp)) != NULL) { //���� �� ���ϵ��� �ϳ��� ����
			if (strcmp(filename, dent->d_name) == 0) { // ���ε��� ������ ����� �ִٸ� 
				search_flag = 1; // flag == 1

				if (arg == 0) {
					printf("%s(��)�� �����մϴ�.\n", filename);
					cpcp(dirname, filename, serv_dirname); // ���� ������ cp 
					printf("%s ���� �Ϸ�.\n\n", filename);
				}
				else if (arg == 1) {
					printf("Send %s.\n", filename);
					cpcp(dirname, filename, serv_dirname); // ���� ������ cp 
					printf("%s Transfer complete. \n\n", filename);
				}

			}
		}
		if (search_flag != 1) { // ���ε��� ������ ���ٸ� 
			if (arg == 0)
				printf("\n>> %s ������ �����ϴ�. \n\n", filename);
			else if (arg == 1)
				printf("\n>> The %s file does not exist. \n\n", filename);
			search_flag = 0;
		}
		closedir(dp);
	}
	else if (option == 5) { // ���� �ٿ�ε� 
		lsls(serv_dirname); // ������ ���� ��� �����ֱ� 
		if (arg == 0)
			printf("�ٿ�ε� �� ���� �� : ");
		else if (arg == 1)
			printf("File name to download : ");

		scanf("%s", filename); getchar();// �ٿ�ε��� ���� ���� �Է¹޾� filename�� ���� 
		if ((dp = opendir(serv_dirname)) == NULL) { //������  ���� open
			perror("opendir: ./");
			exit(1);
		}
		while ((dent = readdir(dp)) != NULL) {
			if (strcmp(filename, dent->d_name) == 0) { // ������ ����� �ִٸ� 
				search_flag = 1;
				if (arg == 0) {
					printf("%s(��)�� �ٿ�ε� �մϴ�.\n", filename);
					cpcp(serv_dirname, filename, dirname); // ������������ �� ������ cp
					printf("%s �ٿ�ε� �Ϸ�. \n\n", filename);
				}
				else if (arg == 1) {
					printf("Download %s.\n", filename);
					cpcp(serv_dirname, filename, dirname); // ������������ �� ������ cp
					printf("%s Download complete.. \n\n", filename);
				}
			}
		}
		if (search_flag != 1) {
			if (arg == 0)
				printf("\n>> %s ������ �����ϴ�. \n\n", filename);
			else if (arg == 1)
				printf("\n>> The %s file does not exist. \n\n", filename);
			search_flag = 0;
		}
		closedir(dp);
	}

	else if (option == 6) {
		lsls(dirname); // �� ���� ��� �����ֱ�
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
			printf("[�ɼ��� �������ּ���]\n");
			printf(">> 1.�ð� �˻� 2.�ܾ� �˻�\n");
		}
		else if (arg == 1) {
			printf("[Please choose an option]\n");
			printf(">> 1.Time Search 2. Word Search\n");
		}

		printf(">> ���� : ");
		scanf("%d", &opt);	getchar();
		if (opt == 1) {
			if (arg == 0)
				printf(">> �˻��� �ð� �Է� (hh:mm) : ");
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
				printf(">> �˻��� �ܾ� �Է� : ");
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
				printf(">> ���ڿ��� ã�� ���߽��ϴ�.\n");
			else if (arg == 1)
				printf(">> String not found. \n");
		printf("===============================================\n\n\n");
		search_flag = 0;
		chdir("..");
		fclose(fp);
	}
}

int connect_cli(char * sock_name) {   //   ���� ���ϸ��� sock_name�����Ͽ� ������ �������ִ� �Լ�.

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
		printf("----- %s ������ ���ϵ� -----\n", dirname);
	else if (arg == 1)
		printf("----- files in %s folder -----\n", dirname);

	for (i = 0; i < n; i++) {
		if (namelist[i]->d_name[0] == '.' || strcmp(namelist[i]->d_name, is_chat) == 0) continue;
		//ä�� �����̳� ��������(.���� ����)�� ���� �ϸ� �ȵǹǷ� ���ܳ��� 
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
	stat(argv1, &st); //st�� argv1�� ���� stat ����
	stat(argv2, &st2); //st2�� argv2�� ���� stat ����


	chdir("..");
	chdir(argv2); // �ش� ���丮�� �̵� �� open 

	if ((fd2 = open(argv2, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) == -1) {
		// fd2 : open argv2, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) : ���� ���縦 ����
		perror("open2");
		exit(0);
	}
	rename(argv2, argv1); // ���� argv1�� �̸� ���� (���丮 ��� ���� �̸����� �����Ǵ� �� ����) 
	chdir("..");

	while ((rd = read(fd1, buf, 10)) > 0) {
		if (write(fd2, buf, rd) != rd) perror("Write"); // fd1 -> fd2 �� ���� ����
		close(fd1);
		close(fd2);
	}
}


//============���� �߰��� �ߺ� Ȯ�� �� ���� ����� �Լ� ====================

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
			printf("�̹� �ִ� �г����̶� ���� ������ -%s- �� �����մϴ�.\n", dirname);
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

void ifexit(void) {	// Ŭ���̾�Ʈ�� ����Ǹ鼭 ó���ؾ��� ������ �����ϴ� �Լ�
	struct dirent *dent;
	DIR *dp;
	/*�������� ���丮 �����*/
	if ((dp = opendir(dirname)) == NULL) { //���� dir open
		perror("opendir: ./");
		exit(1);
	}
	chdir(dirname);
	while ((dent = readdir(dp)) != NULL) {
		remove(dent->d_name); // ���� �� ���ϵ鵵 ���� 
	}
	chdir("..");
	rmdir(dirname); // ���丮 ���� 
	closedir(dp);
}

void tstp_handler(int signo) {
	if (arg == 0)
		printf("\n�����Ͻ÷��� \"ctrl + ��\"�� �Է����ּ��� \n");      // �ñ׳��� ������ ������ �˸���.
	else if (arg == 1)
		printf("\nEnter \"ctrl + ��\" to exit \n");
}
void quit_handler(int signo) {
	send(sd, EXIT_MESSAGE, strlen(EXIT_MESSAGE), 0);	// ������ ���� �޽����� �����Ͽ�, Ŭ���̾�Ʈ�� ���� �������� �˸�.
	if (arg == 0)
		printf("\nŬ���̾�Ʈ�� �����մϴ�...\n");
	else if (arg == 1)
		printf("\nQuit the client ...\n");

	exit(1);	// Ŭ���̾�Ʈ�� ����Ǹ鼭 ó���ؾ��� ������ �����ϴ� �Լ��� ȣ��ǰ�, Ŭ���̾�Ʈ�� ����ȴ�.
}