#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#define MAXLINE 511 // 최대 라인 크기 정의
#define BUFSIZE 256 // 버퍼 크기 정의

// TCP 서버 소켓 생성 및 대기 설정 함수
int tcp_listen(int host, int port, int backlog);

// 오류 발생 시 메시지 출력 후 종료
void errquit(char *mesg) { 
    perror(mesg);
    exit(0);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in cliaddr; // 클라이언트 주소 구조체
    int listen_sock, accp_sock; // 리슨 소켓, 연결된 소켓
    char command[10], filename[MAXLINE], dir[MAXLINE], filepath[MAXLINE], buf[BUFSIZE];
    int addrlen, success = 0, nbyte;
    FILE *file;
    size_t fsize; // 파일 크기 저장 변수

    // 실행 시 포트를 입력하지 않은 경우
    if(argc != 2) {
        printf("사용법 : %s [port]\n", argv[0]);
        exit(0);
    }

    // 서버 소켓 생성 및 대기
    listen_sock = tcp_listen(INADDR_ANY, atoi(argv[1]), 5);
    if(listen_sock == -1) {
        errquit("tcp_listen fail");
    }

    addrlen = sizeof(cliaddr);
    // 클라이언트 연결 수락
    accp_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);

    while(1) {
        // 명령어 수신
        recv(accp_sock, command, sizeof(command), 0);

        // 디렉토리 변경 처리
        if(!strcmp(command, "cd\n")) { 
            recv(accp_sock, dir, MAXLINE, 0); // 디렉토리 경로 수신
            
            // 디렉토리 변경 시도
            if(chdir(dir) == 0) success = 1; 
            else success = 0;

            send(accp_sock, &success, sizeof(int), 0); // 성공 여부 전송
        } 

        // 파일 다운로드 처리
        else if (!strcmp(command, "get\n")) { 
            recv(accp_sock, filename, MAXLINE, 0); // 파일 이름 수신

            // 파일 열기
            file = fopen(filename, "rb"); 

            int isnull = 0; 
            // 파일이 없는 경우 처리
            if(file == NULL) {
                isnull = 1;
                send(accp_sock, &isnull, sizeof(isnull) ,0); 
                continue;
            }
            
            send(accp_sock, &isnull, sizeof(isnull), 0); 

            // 파일 크기 계산
            fseek(file, 0, SEEK_END);
            fsize = ftell(file);
            fseek(file, 0, SEEK_SET);

            // 파일 크기 전송
            size_t size = htonl(fsize); 
            send(accp_sock, &size, sizeof(fsize), 0);

            int nsize = 0;

            // 파일 전송
            while(nsize != fsize) {
                int fpsize = fread(buf, 1, BUFSIZE, file);
                nsize += fpsize;
                send(accp_sock, buf, fpsize, 0);
            }
            fclose(file);
        }

        // 파일 업로드 처리
        else if (!strcmp(command, "put\n")) { 
            int isnull = 0;
            
            recv(accp_sock, &isnull, sizeof(isnull), 0);
            if(isnull == 1) {
                continue;
            }

            recv(accp_sock, filename, MAXLINE, 0); 

            file = fopen(filename, "wb"); // 파일 쓰기 모드로 열기
            recv(accp_sock, &fsize, sizeof(fsize), 0); 

            nbyte = BUFSIZE;
            // 파일 수신 및 저장
            while(nbyte >= BUFSIZE) {
                nbyte = recv(accp_sock, buf, BUFSIZE, 0);
                success = fwrite(buf, sizeof(char), nbyte, file); 
                if(nbyte < BUFSIZE) success = 1;
            }
            
            send(accp_sock, &success, sizeof(int), 0);
            fclose(file);
        }

        // 종료 명령 처리
        else if(!strcmp(command, "quit")) { 
            int isexit = 0;
            recv(accp_sock, &isexit, sizeof(int), 0);

            if(isexit) {
                printf("프로그램을 종료합니다.\n");
                close(listen_sock);
                close(accp_sock);
                exit(0);
            }
        }

        // 파일 조회 (tree 명령 처리)
        else if(!strcmp(command, "tree\n")) {
            FILE *fp;
            fp = popen("tree", "r"); // tree 명령어 실행
            if (fp == NULL) {
                printf("tree 명령어 실행 오류\n");
                continue;
            }

            // 명령어 결과를 클라이언트로 전송
            while(fgets(buf, BUFSIZE, fp) != NULL) {
                send(accp_sock, buf, strlen(buf), 0);
            }
            fclose(fp);
        }

        // 파일 삭제 처리
        else if(!strcmp(command, "delete\n")) {
            // 삭제할 파일 경로 수신
            recv(accp_sock, filepath, MAXLINE, 0);
            recv(accp_sock, filename, MAXLINE, 0);

            char fullpath[MAXLINE];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", filepath, filename); // 전체 경로 생성

            // 파일 삭제
            if(remove(fullpath) == 0) {
                success = 1;
            } else {
                success = 0;
            }

            send(accp_sock, &success, sizeof(int), 0); // 삭제 성공 여부 전송
        }
    }
    return 0;
}

// 서버 소켓 생성 및 설정 함수
int tcp_listen(int host, int port, int backlog) {
    int sd;
    struct sockaddr_in servaddr;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errquit("socket fail");
    }

    // servaddr 구조체 초기화
    bzero((char *)&servaddr, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);
    if(bind(sd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        errquit("bind fail");
    }

    // 클라이언트 연결 요청 대기
    listen(sd, backlog);
    return sd;
}