#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXLINE 512
#define BUFSIZE 1024

typedef struct offset_info {
    int fd;
    int client_sock;
    off_t start; // 시작부분
    off_t end;   // 마지막 부분
    char buffer[1024];
} OFFIN;

void What_I_received(OFFIN off) {
    printf("클라이언트 받은 크기 : %d\n", sizeof(off));
    printf("클라이언트 받은 fd : %d\n", off.fd);
    printf("클라이언트 받은 start : %d\n", off.start);
    printf("클라이언트 받은 end : %d\n", off.end);
    printf("클라이언트 받은 client_sock : %d\n", off.client_sock);
    printf("클라이언트 받은 buffer : %s\n", off.buffer);
}

void *process_range(void *off) {
    OFFIN *off_info = (OFFIN *)off;
    int fd = off_info->fd;
    off_t start = off_info->start;
    off_t end = off_info->end;
    int client_sock = off_info->client_sock;

    lseek(fd, start, SEEK_SET); // 파일 포인터를 시작 위치로 이동

    off_t remaining = end - start;

    while (remaining > 0) {
        // 반복문 한번당 읽어야하는 바이트
        ssize_t byte_to_read = (remaining < 1024) ? remaining : 1024;
        // 실제로 읽어서 버퍼에 저장한 바이트
        ssize_t bytes_read = read(fd, off_info->buffer, byte_to_read);
        // 버퍼에 저장한거 + 바이트 읽은거 구조체에 다 저장해서 send로 보냄.
        // 클라이언트는 send 한거 읽고 범위확인해서 클라이언트 파일에 write
        send(client_sock, off_info->buffer, bytes_read, 0);
        remaining -= bytes_read; // 읽은만큼 총 남은 비트에서 까줌
    }
}

void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("./%s <server ip> 입력하시오...\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int sock;
    struct sockaddr_in servaddr;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        errquit("Socket creation failed");

    // 서버 주소 설정
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        errquit("invalid address\n");
    }

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("connect failed...");
    }
    printf("Connected to server at %s:8080\n", server_ip);

    while (1) {
        char command[5]; // 명령어 저장
        char filename[MAXLINE];
        int fd;            // 파일 디스크립터
        unsigned sentSize; // 파일 받은 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
        unsigned recvSize; // 파일 받은 사이즈
        unsigned fileSize; // 총 파일 사이즈
        unsigned netFileSize; // size_t == unsined 총 파일 사이즈, 네트워크 전송용
        char buf[BUFSIZE];
        int isnull; // 파일 있는지 없는지 여부 판별용 변수

        printf("\nEnter command (get/put/exit): ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        // 종료 명령 처리
        if (strcmp(command, "exit") == 0) {
            printf("exiting client.\n");
            break;
        }

        // 서버로 명령어 전송
        if (send(sock, command, strlen(command), 0) <= 0) {
            errquit("send cmd failed...");
            break;
        }

        if (strcmp(command, "get") == 0) { // 다운로드 잘댐
            /*파일 이름 입력 부분*/
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0'; // 개행 빼주기
            /* 여기까지 */

            send(sock, filename, strlen(filename), 0); // 파일 이름 전송 1

            if (recv(sock, &isnull, sizeof(isnull), 0) <= 0) { // 파일 이름 있는지 없는지 2
                perror("receiving file existence fail");
                continue;
            }

            if (isnull == 1) {
                printf("file not found on server.\n");
                continue;
            }

            if (recv(sock, &netFileSize, sizeof(netFileSize), 0) <= 0) { // 전달 받은 파일 사이즈 3
                perror("receiving file size fail");
                continue;
            }

            fileSize = ntohl(netFileSize);
            printf("downloading file [%s] (%u bytes)\n", filename, fileSize);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                errquit("file open failed");
                continue;
            }

            sentSize = 0;
            while (sentSize < fileSize) {
                OFFIN recv_info;                              // recv_info를 구조체 변수로 선언
                recv(sock, &recv_info, sizeof(recv_info), 0); // 구조체 주소로 recv 호출
                What_I_received(recv_info);
                recvSize = strlen(recv_info.buffer);

                if (recvSize <= 0) {
                    printf("145 리시브 에러");
                    break;
                }

                // 파일 오프셋 이동
                if (lseek(fd, recv_info.start, SEEK_SET) == (off_t)-1) {
                    perror("Failed to seek");
                    close(fd);
                    return 1;
                }

                ssize_t bytes_written = write(fd, recv_info.buffer, strlen(recv_info.buffer));
                if (bytes_written < 0) {
                    perror("Failed to write");
                    close(fd);
                    return 1;
                }
                sentSize += recvSize;
            }

            if (sentSize == fileSize) {
                printf("file [%s] downloaded successfully.\n", filename);
            } else {
                printf("file [%s] download incomplete.\n", filename);
            }
            close(fd);

        } else if (strcmp(command, "put") == 0) {
            memset(filename, 0, sizeof(filename));

            /* 업로드할 파일 이름 입력 */
            printf("enter filename to upload: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0'; // 개행문자 널로 변경
            /* 여기까지 */

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                perror("file open failed");
                isnull = 0;
                continue;
            }

            send(sock, filename, strlen(filename), 0); // 파일 이름 전송 1

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(sock, &netFileSize, sizeof(netFileSize), 0); // 파일 크기 송신 2

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize); // 업로드할 파일 정보 출력

            OFFIN off[4];
            pthread_t pthreads[4];

            int sentSize = 0;

            for (int i = 0; i < 4; i++) { // 파일을 읽어들여서 4등분으로 나누기
                off[i].start = 0 + (fileSize / 4) * i;
                off[i].end = (fileSize / 4) * (i + 1);
                off[i].fd = fd;
                off[i].client_sock = sock;
            }

            for (int i = 0; i < 4; i++) {
                pthread_create(&pthreads[i], NULL, process_range, &off[i]);
            }

            for (int i = 0; i < 4; i++) {
                pthread_join(pthreads[i], NULL);
            }

            if (sentSize == fileSize) {
                printf("file [%s] uploaded successfully.\n", filename);
            } else {
                printf("file [%s] upload incomplete.\n", filename);
            }
            close(fd);
        } else {
            printf("invalid command. Use 'get', 'put', or 'exit'.\n");
        }
    }

    close(sock);
    return 0;
}