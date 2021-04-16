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
#define MAX_USR 20   // �ִ� ����� ��

struct room {   // �� ä�ù��� ������ �����ϴ� ����ü
	char room_name[MAX_LEN];   // ä�ù� �̸�
	char sock_name[MAX_LEN];   // ���� ���ϸ�
	char * user_list[MAX_USR];   // ����ڸ� �迭   	
	int client_s[MAX_SOCK];   // Ŭ���̾�Ʈ�� �����ϴ� ���� ��ũ���� �迭
	int cli_cnt;   // ����� ����� ��
	int user_arg[MAX_USR];	//***** �߰� ����� �ɼ�
	pthread_t thread;   // thread identifier
};

struct pass {   // ������� ���޵Ǵ� ����ü
	int psd;   // �θ� ������ Ŭ���̾�Ʈ�� �����ϴ� ���� ��ũ����
	struct room * cur_room;   // ���� ä�ù濡 ���� ����ü
};


void handler(int signo);
void ifexit();
int connect_cli(char * sock_name);
void get_room_name(int sockfd, char buf[], size_t len, int flags);
int move(int nsd);
void * chat_process(void * p);
void room_list_print(int psd);   // ä�ù� ����� Ŭ���̾�Ʈ���� �����ϴ� �Լ�

struct room room_list[20] = { 0 };   // ���� ������ �����ϴ� ����ü��, �ϴ� �ִ� ���� ������ 20���� ����.
int room_cnt = 0;   // room_cnt�� room�� ����, ���� ������ �ξ move �Լ������� ����� ������ �� �ֵ��� ��.
char *output[] = { "(%R) " };   // �ð� ��� ����
char SOCK_NAME[MAX_LEN];   // Ŭ���̾�Ʈ�� �θ� ������ �����ϴµ� ���Ǵ� ���� ���ϸ�

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
	atexit(ifexit);	// ������ ����� �� ����� �Լ��� ����Ѵ�. 

	signal(SIGPIPE, SIG_IGN);   // ���� ���Ͽ� ���� ��� ���޵Ǵ� �ñ׳� ����.
	signal(SIGTSTP, SIG_IGN);   // ctrl + z ����.
	signal(SIGINT, handler);   // ctrl + c�� handler�� ó���Ͽ� ���� ���� ������ ������. 

	sd = connect_cli(SOCK_NAME); // �θ� ������ Ŭ���̾�Ʈ�� �����ϴ� ������ �����ϰ� Ŭ���̾�Ʈ ��ٸ�.
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
		if (FD_ISSET(sd, &read_fds)) {   // Ŭ���̾�Ʈ�� �θ� ������ �����ϴ� ���
			clen = sizeof(struct sockaddr_un);
			nsd = accept(sd, (struct sockaddr *)&cli, &clen);
			if (nsd != -1) {
				int option;
				// ä�� Ŭ���̾�Ʈ�� ��Ͽ� �߰� 
				client_s[cli_cnt] = nsd;   // Ŭ���̾�Ʈ�� �����ϱ� ���� ���� ��ũ���͸� �迭�� ����.
				cli_cnt++;   // ����Ǿ� �ִ� Ŭ���̾�Ʈ �� 1 ����.
				option = move(nsd);   // Ŭ���̾�Ʈ�� ä�ù����� �̵� ��Ŵ. (Ŭ���̾�Ʈ�� �θ� ������ ����� ������ ���� �̵��ϰų� �����ϴ� �����̴�. )  

				if (option == 0)// Ŭ���̾�Ʈ���� ���� ���� �� �� ����, �̵��� ������ �� �ֵ��� �ϱ� ���� move �Լ������� list�� ����ϰ� ������ �� �ٽ� move�Լ��� ȣ���Ѵ�.
					move(nsd);
			}
		}
	}
	close(sd);
	return 0;
}

int move(int nsd) {   // main �Լ��κ��� ���� �޾� room_list�� move �Լ����� ����.
	struct room * next_room;
	struct pass pass;
	char * send_msg = (char *)malloc(sizeof(char) * MAX_LEN);
	char buf[MAX_LEN];
	char chatfile[MAX_LEN];
	int pid, len;
	int room_num, option;

	sprintf(buf, "%d", room_cnt);   // Ŭ���̾�Ʈ���� �����ϱ� ���� �� ������ ���ڿ��� ��ȯ
	send(nsd, buf, strlen(buf), 0);   // Ŭ���̾�Ʈ���� �� ���� ����.
	len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0'; // Ŭ���̾�Ʈ�κ��� �ɼ� �Է� ����.

	if (strcmp(buf, "room_list_print") == 0) {	// Ŭ���̾�Ʈ�� ������ ó�� ������� �� ���� �̵��� ������ ��쿡�� move�Լ����� list�� ����� �� ��ȯ�ϰ� Ŭ���̾�Ʈ���� ä�ù��� ������ �� �̵����� �Ǵ� ä�ù��� �������� ������ �Ŀ� �ٽ� move�Լ��� ȣ���Ͽ� ���� �ű⵵���Ѵ�.
		room_list_print(nsd);   // ä�ù� ��� ���� �Լ� ����.
		return 0;	// ��ȯ�ϴ� ���� 0�� ��� main�Լ����� move �Լ��� �ٽ� ȣ���Ѵ�.
	}

	option = buf[0] - 48;   // ���ڿ��� ������ �ٲپ� ����.

	if (option == 2) { // Ŭ���̾�Ʈ�� ���� ������ ��û�� ���.
		get_room_name(nsd, buf, MAX_LEN, 0);   // Ŭ���̾�Ʈ�κ��� ���� ���� ä�ù���� �ߺ� Ȯ���� �� �Լ��� ��ȯ�Ǹ� �ߺ����� ���� ä�ù���� buf�� ����ȴ�.
		next_room = &room_list[room_cnt]; // �迭�� room_cnt��° �׸� ���ο� ���� ������ ����.
		strcpy(next_room->room_name, buf); // Ŭ���̾�Ʈ�κ��� ���޹��� ä�ù� �̸����� �ʱ�ȭ
		sprintf(next_room->room_name, "%s", buf); // ���� ä�� ������ ������ ������ �̸� ����.
		sprintf(next_room->sock_name, "%s%d", "sock", room_cnt + 1); // Ŭ���̾�Ʈ�� ������ ���� ���ϸ� ����.
		sprintf(chatfile, "%s_%s.txt", buf, "chat");

		if (mkdir(next_room->room_name, 0766) == -1) { // ä�ù� �̸��� ���� �����
			perror("mkdir");
			exit(1);
		}
		chdir(next_room->room_name);
		creat("server_test_file", 0644);
		creat(chatfile, 0644);
		chdir("..");
		room_cnt++; // �� ���� ����.
	}
	else if (option == 3 || option == 8) { // ���� �̵��ϴ� ���
		if (option == 3)	// Ŭ���̾�Ʈ�� ����� ����Ͽ� ���� �̵��ϴ� ��쿡�� ����� ����Ѵ�.  
			room_list_print(nsd);	// ������ ������ ó�� ������ ��쿡�� ������ ����� ����� ���·� move�Լ��� ȣ���ϱ� ������ ����� ����� �ʿ䰡 ����.

		len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0'; // Ŭ���̾�Ʈ�� �̵��� �� ��ȣ �Է¹���.
		room_num = atoi(buf); // room_num�� ����ڰ� �̵��� �� ��ȣ
		next_room = &room_list[room_num - 1]; // Ŭ���̾�Ʈ�� ������ ä�ù��� ��ȣ�� �ش��ϴ� ����ü�� next_room�� ����.(�������� ���ؼ�)
	}

	pass.psd = nsd;
	pass.cur_room = next_room;   // �����忡 ������ ����ü�� �ʱ�ȭ�Ѵ�.

	if (access(next_room->sock_name, F_OK) != -1) {
		// ���� ������ �̹� ������� �ִ� ��� �ش� ä�ù濡 �̹� ����ڰ� ����� ���±� ������ �����带 ���� �ʿ� ����.
		send(nsd, next_room->sock_name, strlen(next_room->sock_name), 0);   // Ŭ���̾�Ʈ���� ���� ���ϸ� ����.
	}
	else {   // ä�ù濡 ����� Ŭ���̾�Ʈ�� ���� ���
		pthread_create(&room_list->thread, NULL, chat_process, (void *)&pass);   // �����带 �����Ͽ� �� ä�ù��� ���񽺸� ó���Ѵ�.
	}
	return 1;	//  ��ȯ���� 1�� ��쿡�� ��ȯ�� �� �ٽ� move�Լ��� ȣ������ �ʴ´�.
}

void * chat_process(void * p) {
	struct pass * pass = (struct pass *)p;
	struct sockaddr_un cli;
	struct room * cur_room = pass->cur_room;   // ���� ä�ù��� ������ ���� ����ü(�����尡 ���񽺸� �����Կ� ���� ������ �����Ͽ� main �����忡�� ����� �� �ֵ��� �Ѵ�.)
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

	*cli_cnt = 0;	// ����� Ŭ���̾�Ʈ �� �ʱ�ȭ
	sd = connect_cli(cur_room->sock_name);   // ����ü�� ����� ���� ���ϸ����� ������ �����ϰ� Ŭ���̾�Ʈ�� ��ٸ���.
	send(psd, cur_room->sock_name, strlen(cur_room->sock_name), 0);   // Ŭ���̾�Ʈ���� ���� ���ϸ� ����.(Ŭ���̾�Ʈ�� �ش� ���Ͽ� ������ �� �ֵ��� ������.)
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
				len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0';// Ŭ���̾�Ʈ�κ��� ���� �̸� ���� ����.
				cur_room->user_list[*cli_cnt] = (char *)malloc(sizeof(char) * MAX_LEN);
				strcpy(cur_room->user_list[*cli_cnt], buf);   // ���� �̸��� �迭�� �����Ͽ� ����.
				send(nsd, cur_room->room_name, strlen(cur_room->room_name), 0);   // ����� Ŭ���̾�Ʈ���� ä�ù� �̸� ����(ȭ�鿡 ä�ù� �̸��� ����ϴµ� ���ȴ�.)

				len = recv(nsd, buf, MAX_LEN, 0); buf[len] = '\0';// Ŭ���̾�Ʈ�κ��� arg ���޹���.//***** �߰�
				cur_room->user_arg[*cli_cnt] = atoi(buf); //***** �߰�
				// Ŭ���̾�Ʈ�� ��Ͽ� �߰�
				client_s[*cli_cnt] = nsd;   // ����� Ŭ���̾�Ʈ�� ���� ��ũ���͸� �迭�� �����Ͽ� ����.

				(*cli_cnt)++;   // ����� Ŭ���̾�Ʈ�� ���� 1 ������Ų��.

				for (i = 0; i < *cli_cnt; i++) {   // ä�ù� �����ڵ� ��ο��� ���� �޽����� �����Ѵ�.
					if (cur_room->user_arg[i] == 0) //****** �߰�
						sprintf(sender, "\t\t**** %s���� �����ϼ̽��ϴ� ****", cur_room->user_list[*cli_cnt - 1]);   // ���� �̸��� ���� �޽����� �����Ͽ� ����
					else if (cur_room->user_arg[i] == 1)
						sprintf(sender, "\t\t**** %s has joined ****", cur_room->user_list[*cli_cnt - 1]);   // ���� �̸��� ���� �޽����� �����Ͽ� ����
					send(client_s[i], sender, strlen(sender), 0);
				}
			}
		}
		for (i = 0; i < *cli_cnt; i++) {
			if (FD_ISSET(client_s[i], &read_fds)) {
				if ((len = recv(client_s[i], buf, MAX_LEN, 0)) > 0) {	// Ŭ���̾�Ʈ�κ��� �޽����� ���޹޴� ���
					buf[len] = '\0';	// Ŭ���̾�Ʈ�� NULL�� ������ ���ڿ��� �����ϱ� ������ ���� NULL�� �����Ѵ�.
					if (strcmp(buf, "room_list_print") == 0) {   // ����ڰ� ä�ù� ����� ����� ���ϴ� ��� ����� ��������.
						sprintf(sender, "%d", room_cnt);   // ä�ù��� ������ Ŭ���̾�Ʈ���� �����ϱ� ���� ���ڿ��� ��ȯ
						send(client_s[i], sender, strlen(sender), 0);   // ���� ���� ����.
						room_list_print(client_s[i]);   // ä�ù� ��� ���� �Լ� ����.
					}
					else if (strcmp(buf, "clear") == 0) {
						send(client_s[i], cur_room->room_name, strlen(cur_room->room_name), 0);   // ä�ù� �̸� ����, Ŭ���̾�Ʈ���� ä�ù��� clear�� �� ȭ�鿡 ���� �޽����� ����� �� ���ȴ�.
					}
					else {
						if (strstr(buf, "exit") != NULL) {         //  ���Ṯ�� �Է½� ä�� Ż�� ó��
							shutdown(client_s[i], 2);   // ä�ù��� ���� Ŭ���̾�Ʈ�� �����ϴ� ���� ����.
							if (i != *cli_cnt - 1) {   // ���� �������� ����� Ŭ���̾�Ʈ�� �ƴ϶��
								client_s[i] = client_s[*cli_cnt - 1];   // ���� ������ �ε����� ������ �ε����� ������ ����.
								strcpy(cur_room->user_list[i], cur_room->user_list[*cli_cnt - 1]); // �������� ������ �迭�� �Ȱ��� ����.
								cur_room->user_arg[i] = cur_room->user_arg[*cli_cnt - 1];
							}
							(*cli_cnt)--;   // ����� Ŭ���̾�Ʈ�� ���� 1 ���� ��Ų��.
							if (*cli_cnt == 0) {
								// ����� Ŭ���̾�Ʈ�� ���̻� ������ ���� ���� ����(Ŭ���̾�Ʈ�� SIGTSTP, SIGINT�δ� 
								// ������ �� ������, SIGQUIT���� �����ص� ������ ����޽����� �����ϵ��� ����.
								remove(cur_room->sock_name);   // �ش� ���� ������ ����.
							}
							for (int j = 0; j < *cli_cnt; j++) {   // ä�ù� �����ڵ� ��ο��� ���� �޽����� �����Ѵ�.
								if (j != i) {
									if (cur_room->user_arg[j] == 0)
										sprintf(sender, "\t\t**** %s���� �������ϴ� ****", cur_room->user_list[i]);  // ���� �̸��� ���� �޽����� �����Ͽ� ����
									else if (cur_room->user_arg[j] == 1)
										sprintf(sender, "\t\t**** %s left the room ****", cur_room->user_list[i]);  // ���� �̸��� ���� �޽����� �����Ͽ� ����

									send(client_s[j], sender, strlen(sender), 0);
								}
							}
						}
						else {	// ��� ä�� �����ڿ��� �޽��� ����
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
		for (i = 0; i < room_cnt; i++) {   // ���� ������ ��� ������.
			remove(room_list[i].sock_name);
		}
		remove(SOCK_NAME);   // Ŭ���̾�Ʈ�� �θ� ������ �����ϴ� socket ����.

		for (j = 0; j < room_cnt; j++) {
			if ((dp = opendir(room_list[j].room_name)) == NULL) { //���� dir open
				perror("opendir: ./");
				exit(1);
			}
			chdir(room_list[j].room_name);
			while ((dent = readdir(dp)) != NULL) {
				remove(dent->d_name); // ���� �� ���ϵ鵵 ���� 
			}
			chdir("..");
			rmdir(room_list[j].room_name); // ���丮 ���� 
		}
		for (int i = 0; i < room_cnt; i++) {   // ������ ����Ǹ� ��� Ŭ���̾�Ʈ���� ������ ����Ǿ����� �˸��� Ŭ���̾�Ʈ�� �����Ų��.
			for (int j = 0; j < room_list[i].cli_cnt; j++) {
				send(room_list[i].client_s[j], "exit", strlen("exit"), 0);
				close(room_list[i].client_s[j]);
			}
		}
		remove(SOCK_NAME);   // �θ� ������ Ŭ���̾�Ʈ�� �����ϴ� ���� ���ϵ� ����.
	 //==========12/6 �߰�========//
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

	for (i = 0; i < room_cnt; i++) {   // Ŭ���̾�Ʈ���� � ä�ù��� �ִ��� �˷���.
		sprintf(buf, "%d. %s (%d)", i + 1, room_list[i].room_name, room_list[i].cli_cnt);	// ä�ù� ��ȣ, ä�ù� �̸�, ������ ���� �̾���δ�.
		for (j = 0; j < room_list[i].cli_cnt; j++) {	// �ش� ä�ù��� Ŭ���̾�Ʈ �� ��ŭ �����ϸ鼭 �������� �г����� ���ڿ��� �ٿ��ش�.
			if (j == 0)
				strcat(buf, "[");
			strcat(buf, room_list[i].user_list[j]);
			if (j == room_list[i].cli_cnt - 1)
				strcat(buf, "]");
			else
				strcat(buf, ", ");
		}
		send(psd, buf, strlen(buf), 0);	// ó���� ���� ���ڿ��� Ŭ���̾�Ʈ�� �����Ѵ�.
	}
}

void get_room_name(int sockfd, char buf[], size_t len, int flags) {   // ����ڷκ��� ���޹��� ä�ù� �̸��� �̹� �����ϴ��� Ȯ���Ͽ�, �� ����� Ŭ���̾�Ʈ���� �˸���. 
																	  // �ߺ��� ��� Ŭ���̾�Ʈ�� ����ڷκ��� ä�ù� �̸��� �ٽ� �Է¹ް�, �ߺ����� ���� ��� ä�ù��� �ش� �̸����� �����Ѵ�.
	int flag = 0;
	int length = len;

	do {
		len = recv(sockfd, buf, length, flags);   buf[len] = '\0'; // Ŭ���̾�Ʈ�κ��� ä�ù� �̸� ���� ����.
		flag = 0;   // �ߺ� ���θ� ��Ÿ���� �÷���
		for (int i = 0; i < room_cnt; i++) {   // ä�ù� ������ŭ Ȯ��.
			if (strcmp(buf, room_list[i].room_name) == 0) {   // ���� �̸��� ä�ù��� �����Ѵٸ�.
				flag = 1;   // �÷��׸� �����Ͽ� �ݺ��ǵ��� ��.
				strcpy(buf, "fail");
				send(sockfd, buf, strlen(buf), 0);   // fail �޽��� ����, Ŭ���̾�Ʈ�� �� �޽����� ���޹޾� �ߺ� ���θ� Ȯ���ϰ� �׿� ���� ����ڿ��� �ٽ� ä�ù� �̸��� �Է¹޴´�.
				break;
			}
		}
	} while (flag == 1);

	send(sockfd, buf, strlen(buf), 0);   // �ߺ����� ���� ä�ù� �̸��̶�� Ŭ���̾�Ʈ�� ������ ���ڿ��� �״�� ���������ν� �ߺ����� ���� ä�ù� �̸����� �˸���.
	len = recv(sockfd, buf, length, flags);   buf[len] = '\0';// ������ ä�ù� �̸��� �ٽ� ���� �޾� Ŭ���̾�Ʈ�� ���������� Ȯ���Ѵ�.
															  // (�� ������ �߰����� ������ ���� send �ڵ忡�� sock_name���� ���޵Ǵ� ������ ���� ���Ͽ� �߰��� �����̴�.)
}

int connect_cli(char * sock_name) {   //   ���� ���ϸ��� sock_name�����Ͽ� ������ �����ϰ� Ŭ���̾�Ʈ�� ��ٸ�.
	struct sockaddr_un ser, cli;
	int sd, len, clen;

	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {   // ���� ����
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

void handler(int signo) {   // ctrl + c �Է��Ͽ� ������ �����Ű�� ���
	exit(1);   // ������� atexit �Լ��� ���� ��Ͻ�Ų ifexit �Լ��� �����ϵ��� �Ѵ�.
}