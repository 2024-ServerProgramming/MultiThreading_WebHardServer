#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#define MAXLINE 511
#define BUFSIZE 256

int tcp_connect(char * ip, int host); // 소켓 생성 및 listen
void errquit(char *mesg) {
	perror(mesg);
	exit(0);
}

int main(int argc, char *argv[]) {
	int listen_sock;
	char command[5], filename[MAXLINE], dir[MAXLINE], temp[5], buf[BUFSIZE];
	int success = 0, nbyte;
	FILE *file;
	size_t fsize;

	if(argc != 3) {
		printf("사용법 : %s [server_ip] [port]\n", argv[0]);
		exit(0);
	}

	listen_sock = tcp_connect(argv[1], atoi(argv[2]));
	
	if (listen_sock == -1) {
		errquit("tcp_connect fail");
	}

	while(1) {
		printf("\n< 명령어 : cd, get, put, quit >\n");
		printf("[command] ");
		fgets(command, 5, stdin); // 명령어 입력
		send(listen_sock, command, 5, 0); // 명령어 전송

		if (!strcmp(command, "cd\n")) { // cd 명령어 처리
			printf("이동을 원하는 경로 : ");
			scanf("%s", dir); // 경로 입력
			fgets(temp, 5, stdin); // 버퍼에 남은 \n 제거

			send(listen_sock, dir, MAXLINE, 0); // 경로 전송
			recv(listen_sock, &success, sizeof(int), 0); //성공 여부 수신

			if (success) printf("경로가 변경되었습니다.\n");
			else printf("경로 변경에 실패하였습니다.\n");
		}
		else if (!strcmp(command, "get\n")) { // get 명령어 처리(다운로드)
			printf("다운로드할 파일명 : ");
			scanf("%s", filename); // 파일 이름 입력
			fgets(temp, 5, stdin); // 버퍼에 남은 \n 제거
			send(listen_sock, filename, MAXLINE, 0); // 파일 이름 전송

			int isnull = 0;
			recv(listen_sock, &isnull, sizeof(isnull), 0);
			if(isnull == 1) {
				printf("해당 파일이 존재하지 않습니다.\n");
				continue;
			}

			recv(listen_sock, &fsize, sizeof(fsize), 0); // 파일 크기 수신
			file = fopen(filename, "wb"); // 쓰기 전용, 이진 모드로 파이 열기

			nbyte = BUFSIZE;
			while(nbyte >= BUFSIZE) {
				nbyte = recv(listen_sock, buf, BUFSIZE, 0); // 256바이트씩 수신
				fwrite(buf, sizeof(char), nbyte, file); // 파일 쓰기
			}
			fclose(file);
			printf("다운로드가 완료되었습니다.\n"); // 다운로드 성공
		}
		else if(!strcmp(command, "put\n")) { // put 명령어 처리(업로드)
			printf("업로드할 파일명 : ");
			scanf("%s", filename); // 파일 이름 입력
			fgets(temp, 5, stdin); // 버퍼에 남은 \n 제거

			file = fopen(filename, "rb"); // 읽기 전용, 이진 모드로 파일 열기

			int isnull = 0;
			if (file == NULL) {
				isnull = 1;
				send(listen_sock, &isnull, sizeof(isnull), 0);
				printf("해당 파일이 존재하지 않습니다.\n");
				continue;
			}
			send(listen_sock, &isnull, sizeof(isnull), 0); // 파일 존재 여부 전송

			send(listen_sock, filename, MAXLINE, 0); // 파일 이름 전송

			/*파일 크기 계산*/
			fseek(file, 0, SEEK_END); // 파일 포이너를 끝으로 이동
			fsize = ftell(file); // 파일 크기 계산
			fseek(file, 0, SEEK_SET); // 파일 포인터를 다시 처음으로 이동

			size_t size = htonl(fsize);
			send(listen_sock, &size, sizeof(fsize), 0); // 파일 크기 전송

			int nsize = 0;

			/*파일 전송*/
			while(nsize != fsize) { // 256 바이트씩 전송
				int fpsize = fread(buf, 1, BUFSIZE, file);
				nsize += fpsize;
				send(listen_sock, buf, fpsize, 0);
			}
			fclose(file);
			recv(listen_sock, &success, sizeof(int), 0); // 업로드 성공 여부 수신

			if (success) printf("업로드가 완료되었습니다.\n");
			else printf("업로드를 실패했습니다.\n");
		}
		else if(!strcmp(command, "quit")) { // quit 명령어 처리(종료)
			int isexit = 1;
			send(listen_sock, &isexit, sizeof(int), 0);
			printf("프로그램을 종료합니다.\n");
			close(listen_sock);
			exit(0);
		}
	}
}

int tcp_connect(char *ip, int port) {
	int sd;
	struct sockaddr_in servaddr;

	// 소켓 생성
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		errquit("socket fail");
	}

	// 서버의 소켓주소 구조체 servaddr 내용 세팅
	bzero((char *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	// 연결 요청
	if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		errquit("connect fail");
	}
	return sd;
}
