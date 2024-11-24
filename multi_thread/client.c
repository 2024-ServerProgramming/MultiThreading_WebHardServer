#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#define MAXLINE 512
#define BUFSIZE 256

void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("./%s <server ip> 입력하시오...\n", argv[0]);
        exit(1);
    }

    int sock;
    struct sockaddr_in servaddr;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) errquit("Socket creation failed");

    // 서버 주소 설정
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    servaddr.sin_addr.s_addr = htonl(argv[1]);

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("connect failed...");
    }
    printf("Connected to server at %s:8080\n", server_ip);

    while (1) {
        char command[5];
        char filename[MAXLINE];
        int fd, fileSize, receivedSize, readSize;
        char buf[BUFSIZE];
        int isnull;

        printf("\nEnter command (get/put/exit): ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0'; // 개행 문자 제거

        // 종료 명령 처리
        if (strcmp(command, "exit") == 0) {
            printf("exiting client.\n");
            break;
        }

        // 서버로 명령어 전송
        if (send(sock, command, strlen(command) + 1, 0) <= 0) {
            errquit("send cmd failed...");
            break;
        }

        if (strcmp(command, "get") == 0) {
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            send(sock, filename, strlen(filename) + 1, 0);

            if (recv(sock, &isnull, sizeof(isnull), 0) <= 0) {
                errquit("receiving file existence fail");
                continue;
            }

            if (isnull == 0) {
                printf("file not found on server.\n");
                continue;
            }

            if (recv(sock, &fileSize, sizeof(fileSize), 0) <= 0) {
                errquit("receiving file size fail");
                continue;
            }

            fileSize = ntohl(fileSize);
            printf("downloading file [%s] (%d bytes)\n", filename, fileSize);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                errquit("file open failed");
                continue;
            }

            receivedSize = 0;
            while (receivedSize < fileSize) {
                readSize = recv(sock, buf, BUFSIZE, 0);
                if (readSize <= 0) break;
                write(fd, buf, readSize);
                receivedSize += readSize;
            }

            if (receivedSize == fileSize) {
                printf("file [%s] downloaded successfully.\n", filename);
            } else {
                printf("file [%s] download incomplete.\n", filename);
            }
            close(fd);

        } else if (strcmp(command, "put") == 0) {
            printf("enter filename to upload: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                errquit("file open failed");
                isnull = 0;
                send(sock, &isnull, sizeof(isnull), 0);
                continue;
            }

            isnull = 1;
            send(sock, &isnull, sizeof(isnull), 0);
            send(sock, filename, strlen(filename) + 1, 0);

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            int netFileSize = htonl(fileSize);
            send(sock, &netFileSize, sizeof(netFileSize), 0);

            printf("Uploading file [%s] (%d bytes)\n", filename, fileSize);

            int sentSize = 0;
            while (sentSize < fileSize) {
                readSize = read(fd, buf, BUFSIZE);
                if (readSize <= 0) break;
                send(sock, buf, readSize, 0);
                sentSize += readSize;
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
