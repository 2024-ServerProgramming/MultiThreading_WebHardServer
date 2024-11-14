#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#define MAXLINE 511
#define BUFSIZE 256

int tcp_listen(int host, int port, int backlog); // 소켓 생성 및 listen

void errquit(char *mesg) { // 런타임 에러 함수
	perror(mesg);
	exit(0);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in cliaddr;
	int listen_sock, accp_sock;
	char command[5], filename[MAXLINE], dir[MAXLINE], buf[BUFSIZE];
	int addrlen, success = 0, nbyte;
	FILE *file;
	size_t fsize; // 파일 크기 저장, == unsigned int

	if(argc != 2) { // 실행 시 포트 입력 안 했을 경우
		printf("사용법 : %s [port]\n", argv[0]);
		exit(0);
	}

	listen_sock = tcp_listen(INADDR_ANY, atoi(argv[1]), 5);
	if(listen_sock == -1) {
		errquit("tcp_listen fail");
	}

	addrlen = sizeof(cliaddr);
	accp_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);

	while(1) {
		recv(accp_sock, command, 5, 0); // 명령어 수신

		/*여기서부터 클라이언트한테 명령 받고 처리 (다운, 업로드 등)*/
		if(!strcmp(command, "cd\n")) { // cd 명령어 수신 처리
			recv(accp_sock, dir, MAXLINE, 0); // 경로 수신
			
			if(chdir(dir) == 0) success = 1; //경로 변경 성공
			else success = 0;

			send(accp_sock, &success, sizeof(int), 0); // 성공 여부 송신
		} else if (!strcmp(command, "get\n")) { // get 명령어 수신 처리(다운로드)
			recv(accp_sock, filename, MAXLINE, 0); //파일 이름 수신

			file = fopen(filename, "rb"); // 읽기 전용, 이진 모드로 파일 열기

			int isnull = 0;
			if(file == NULL) {
				isnull = 1;
				send(accp_sock, &isnull, sizeof(isnull) ,0); // 파일이 빈 경우 송신 처리
				continue;
			}
			
			send(accp_sock, &isnull, sizeof(isnull), 0); // 파일 존재 여부 송신

			/*파일 크기 계산*/
			fseek(file, 0, SEEK_END); // 파일 포인터를 끝으로 이동
			fsize = ftell(file); // 파일 크기 계산
			fseek(file, 0, SEEK_SET); // 파일 포인터를 다시 처음으로 이동

			// TCP/IP 네트워크 파이트 정렬인 32 비트 정수값을 입력으로 
			// 호스트 바이트 정렬인 32비트 정수 값을 리턴
			size_t size = htonl(fsize); 
			send(accp_sock, &size, sizeof(fsize), 0); //파일 크기 송신

			int nsize = 0;

			/*파일 전송*/
			while(nsize != fsize) { // 256 바이트씩 송신
				int fpsize = fread(buf, 1, BUFSIZE, file);
				nsize += fpsize;
				send(accp_sock, buf, fpsize, 0);
			} fclose(file);
		}

		else if (!strcmp(command, "put\n")) { // put 명령어 처리 (업로드)
			int isnull = 0;
			
			recv(accp_sock, &isnull, sizeof(isnull), 0);
			if(isnull == 1) {
				continue;
			}

			recv(accp_sock, filename, MAXLINE, 0); // 파일 이름 수신

			file = fopen(filename, "wb");
			recv(accp_sock, &fsize, sizeof(fsize), 0); // 파일 크기 수신

			nbyte = BUFSIZE;
			while(nbyte >= BUFSIZE) {
				nbyte = recv(accp_sock, buf, BUFSIZE, 0); // 256바이트씩 수신
				success = fwrite(buf, sizeof(char), nbyte, file); // 파일 쓰기
				if(nbyte < BUFSIZE) success = 1;
			}
			
			send(accp_sock, &success, sizeof(int), 0); // 성공 여부 전송
			fclose(file);
		}

		else if(!strcmp(command, "quit")) { // quit 명령어 처리(종료)
			int isexit = 0;
			recv(accp_sock, &isexit, sizeof(int), 0);

			if(isexit) {
				printf("프로그램을 종료합니다.\n");
				close(listen_sock);
				close(accp_sock);
				exit(0);
			}
		}
	}
	return 0;
}

int tcp_listen(int host, int port, int backlog) {
	int sd;
	struct sockaddr_in servaddr;

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		errquit("socket fail");
	}

	// servaddr 구조체의 내용 세팅
	bzero((char *)&servaddr, sizeof(servaddr)); //값을 0으로 초기화해주는 함수
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(host);
	servaddr.sin_port = htons(port);
	if(bind(sd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		errquit("bind fail");
	}

	// 클라이언트로부터 연결 요청을 대기
	listen(sd, backlog);
	return sd;
}
