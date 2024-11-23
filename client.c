#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define MAXLINE 511
#define BUFSIZE 256
void errquit(char *mesg) {
    perror(mesg);
    exit(0);
}

int tcp_connect(char *ip, int port) {
    int sd;
    struct sockaddr_in servaddr; // 소켓 정보들 저장할 구조체 servaddr

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // 소켓 생성 디스크립터 이름 sd
        errquit("socket fail");
    }

    bzero((char *)&servaddr, sizeof(servaddr)); // servaddr 초기화
    servaddr.sin_family = AF_INET;              // IPv4 인터넷 프로토콜
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    // 클라이언트는 ip를 문자열로 받으므로 정규화 이렇게함 그렇게 한걸 servaddr.sin_addr에
    // 이진 빅엔디언 형식으로 저장
    servaddr.sin_port = htons(port); // 포트번호 빅엔디언으로 변경

    if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        // 클라이언트 소켓 sd로 서버 소켓주소 정보 servaddr를 통해서 연결요청함.
        errquit("connect fail");
    }
    return sd;
}

int main(int argc, char *argv[]) {
    int listen_sock;
    char command[5], filename[MAXLINE], dir[MAXLINE], temp[5], buf[BUFSIZE];
    int success = 0, nbyte;
    FILE *file;
    size_t fsize;

    if (argc != 3) { // 파라미터 3개아니면 컷 (실행파일 합쳐서)
        printf("사용법 : %s [server_ip] [portnum]\n", argv[0]);
        exit(0);
    }

    listen_sock = tcp_connect(argv[1], atoi(argv[2])); // 파라미터로 ip와 포트를 받음
    // listen_sock 에는 클라이언트 소켓 디스크립터 저장
    if (listen_sock == -1)
        errquit("tcp_connect fail");

    while (1) {
        printf("\n< 명령어 : cd, get, put, quit >\n");
        printf("[command]");
        fgets(command, 5, stdin);         // 널문자 포함해서 5개 입력받을게 (명렁어)
        send(listen_sock, command, 5, 0); // 클라이언트 소켓 타고 명령어 보냄, 0 은 일반적인 전송
        while (getchar() != '\n')
            ; // 버퍼지우기

        if (!strcmp(command, "cd\n")) {
            printf("이동을 원하는 경로 : ");
            scanf("%s", dir);      // 경로 입력
            fgets(temp, 5, stdin); // 버퍼에 남은 \n 제거

            send(listen_sock, dir, MAXLINE, 0);          // 경로전송
            recv(listen_sock, &success, sizeof(int), 0); // 성공여부 확인
            if (success)
                printf("경로가 변경되었습니다.\n");
            else
                printf("경로 변경에 실패하였습니다.\n");
        } else if (!strcmp(command, "get\n")) {
            printf("다운로드할 파일명 : ");
            scanf("%s", filename);                   // 클라이언트한테 파일 이름 입력받고
            fgets(temp, 5, stdin);                   // 버퍼에 \n 남아있을거니 그거 제거
            send(listen_sock, filename, MAXLINE, 0); // 파일 이름 전송

            int isnull = 0;
            recv(listen_sock, &isnull, sizeof(isnull), 0);
            if (isnull == 1) {
                printf("해당 파일이 존재하지 않습니다.\n");
                continue;
            }

            recv(listen_sock, &fsize, sizeof(fsize), 0); // 파일크기 수신
            file = fopen(filename, "wb");                // 쓰기전용, 이진모드로 파일열기

            nbyte = BUFSIZE;
            while (nbyte >= BUFSIZE) {
                nbyte = recv(listen_sock, buf, BUFSIZE, 0);
                fwrite(buf, sizeof(char), nbyte, file);
            }
            fclose(file);
            printf("다운로드가 완료되었습니다.\n"); // 다운로드 성공
        } else if (!strcmp(command, "put\n")) {     // 파일 업로드
            printf("업로드할 파일명 : ");
            scanf("%s", filename); // 파일 이름 입력
            fgets(temp, 5, stdin); // 버퍼에 남은 \n 제거

            file = fopen(filename, "rb"); // 읽기전용, 이진 모드로 파일열기

            int isnull = 0;
            if (file == NULL) {
                isnull = 1;
                send(listen_sock, &isnull, sizeof(isnull), 0);
                printf("해당 파일이 존재하지 않습니다.\n");
                continue;
            }
            send(listen_sock, &isnull, sizeof(isnull), 0);
            send(listen_sock, filename, MAXLINE, 0);

            /*파일 크기 계산*/
            fseek(file, 0, SEEK_END); // 파일 포인터 파일 끝으로
            fsize = ftell(file);      // 파일 포인터 시작기준 오프셋 읽음 (파일 크기)
            fseek(file, 0, SEEK_SET); // 파일 포인터를 다시 처음으로 이동

            size_t size = htonl(fsize);
            send(listen_sock, &size, sizeof(size), 0);

            int nsize = 0;
            /*파일 전송*/
            while (nsize != fsize) { // 파일 사이즈 될때까지 반복하겠다. (다 보내겠다.)
                int fpsize = fread(buf, 1, BUFSIZE, file); // 파일을 1바이트씩 bufsize 만큼 읽어서 buf에 저장.
                // fpsize에 저장하는 이유는 send 함수에 사용하려고
                nsize += fpsize; // nsize는 내가 서버에 파일내용을 몇 바이트나 보냈는지 기록
                send(listen_sock, buf, fpsize, 0); // 버퍼에 저장한거 클라이언트 소켓을 통해서 서버로 보냄
            }
            fclose(file);
            recv(listen_sock, &success, sizeof(success), 0);

            if (success) {
                printf("파일 업로드가 완료되었습니다.\n");
            } else {
                printf("파일 업로드를 실패했습니다.\n");
            }
        } else if (!strcmp(command, "quit")) {
            int isexit = 1;
            send(listen_sock, &isexit, sizeof(isexit), 0);
            printf("프로그램을 종료합니다.\n");
            close(listen_sock);
            exit(0);
        }
    }
}
