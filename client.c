#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXLINE 511 // 최대 메시지 크기
#define BUFSIZE 256 // 파일 전송 시 사용하는 버퍼 크기

// 소켓 연결 함수
int tcp_connect(char *ip, int port);

// 에러 메시지 출력 후 종료
void errquit(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int sock;               // 서버와 연결된 소켓
    char command[MAXLINE];  // 사용자 명령어 저장
    char filename[MAXLINE], filepath[MAXLINE], dir[MAXLINE], buf[BUFSIZE];
    int success = 0;

    // 서버 IP와 포트 입력 확인
    if (argc != 3) {
        printf("사용법: %s [서버 IP] [포트번호]\n", argv[0]);
        exit(1);
    }

    // 서버에 연결
    sock = tcp_connect(argv[1], atoi(argv[2]));

    // 클라이언트 명령 루프
    while (1) {
        printf("명령어 입력 (cd, get, put, tree, delete, quit): ");
        fgets(command, sizeof(command), stdin);

        // 서버에 명령어 전송
        send(sock, command, strlen(command), 0);

        // 종료 명령 처리
        if (strncmp(command, "quit", 4) == 0) {
            printf("클라이언트를 종료합니다.\n");
            break;
        }

        // 디렉토리 변경 처리
        else if (strncmp(command, "cd", 2) == 0) {
            printf("변경할 디렉토리 경로 입력: ");
            fgets(dir, sizeof(dir), stdin);
            dir[strcspn(dir, "\n")] = 0;  // 개행 문자 제거

            send(sock, dir, sizeof(dir), 0);  // 서버에 디렉토리 경로 전송

            recv(sock, &success, sizeof(success), 0);  // 결과 수신
            if (success) printf("디렉토리 변경 성공\n");
            else printf("디렉토리 변경 실패\n");
        }

        // 파일 다운로드 (get)
        else if (strncmp(command, "get", 3) == 0) {
            printf("다운로드할 파일 이름 입력: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;  // 개행 문자 제거

            send(sock, filename, sizeof(filename), 0);  // 서버에 파일 이름 전송

            int file_exists;
            recv(sock, &file_exists, sizeof(file_exists), 0);  // 파일 존재 여부 수신

            if (!file_exists) {
                printf("서버에서 파일을 찾을 수 없습니다.\n");
                continue;
            }

            size_t file_size;
            recv(sock, &file_size, sizeof(file_size), 0);  // 파일 크기 수신

            FILE *fp = fopen(filename, "wb");
            if (!fp) {
                perror("파일 생성 실패");
                continue;
            }

            printf("파일 다운로드 중...\n");
            size_t received = 0;
            while (received < file_size) {
                int n = recv(sock, buf, BUFSIZE, 0);
                fwrite(buf, 1, n, fp);
                received += n;
            }
            fclose(fp);
            printf("파일 다운로드 완료: %s\n", filename);
        }

        // 파일 업로드 (put)
        else if (strncmp(command, "put", 3) == 0) {
            printf("업로드할 파일 이름 입력: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;  // 개행 문자 제거

            FILE *fp = fopen(filename, "rb");
            if (!fp) {
                printf("파일을 찾을 수 없습니다: %s\n", filename);
                int file_not_found = 1;
                send(sock, &file_not_found, sizeof(file_not_found), 0);
                continue;
            }

            int file_not_found = 0;
            send(sock, &file_not_found, sizeof(file_not_found), 0);
            send(sock, filename, sizeof(filename), 0);  // 서버에 파일 이름 전송

            fseek(fp, 0, SEEK_END);
            size_t file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            send(sock, &file_size, sizeof(file_size), 0);  // 서버에 파일 크기 전송

            printf("파일 업로드 중...\n");
            size_t sent = 0;
            while (sent < file_size) {
                int n = fread(buf, 1, BUFSIZE, fp);
                send(sock, buf, n, 0);
                sent += n;
            }
            fclose(fp);

            recv(sock, &success, sizeof(success), 0);  // 업로드 결과 수신
            if (success) printf("파일 업로드 성공: %s\n", filename);
            else printf("파일 업로드 실패\n");
        }

        // 파일 조회 (tree)
        else if (strncmp(command, "tree", 4) == 0) {
            printf("서버 파일 구조:\n");
            while (1) {
                int n = recv(sock, buf, BUFSIZE, 0);
                if (n <= 0) break;
                buf[n] = '\0';
                printf("%s", buf);
            }
        }

        // 파일 삭제 (delete)
        else if (strncmp(command, "delete", 6) == 0) {
            printf("삭제할 파일의 경로 입력: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\n")] = 0;  // 개행 문자 제거

            printf("삭제할 파일 이름 입력: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;  // 개행 문자 제거

            send(sock, filepath, sizeof(filepath), 0);  // 서버에 경로 전송
            send(sock, filename, sizeof(filename), 0);  // 서버에 파일 이름 전송

            recv(sock, &success, sizeof(success), 0);  // 삭제 결과 수신
            if (success) printf("파일 삭제 성공: %s/%s\n", filepath, filename);
            else printf("파일 삭제 실패\n");
        }
    }

    close(sock);
    return 0;
}

// TCP 소켓 연결 함수
int tcp_connect(char *ip, int port) {
    int sd;
    struct sockaddr_in servaddr;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errquit("소켓 생성 실패");
    }

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("서버 연결 실패");
    }
    return sd;
}