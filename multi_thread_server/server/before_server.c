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
#define BUFSIZE 256

void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

int tcp_listen(int host, int port, int backlog) {
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);

    if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("bind failed...");
    }

    if (listen(sd, backlog) < 0) {
        errquit("listen failed...");
    }

    return sd;
}

void *client_handler(void *input) {
    int client_sock = *(int *)input;
    free(input);

    char command[5]; // 명령어 저장
    char filename[MAXLINE];
    char buf[BUFSIZE];
    int fd;               // 파일 디스크립터
    unsigned fileSize=0;    // 파일 송수신할 때 총 파일 사이즈
    unsigned sentSize=0;    // 파일 보낸 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
    unsigned recvSize;    // 파일 받은 사이즈
    unsigned netFileSize; // size_t == unsigned, 네트워크 전송용
    int isnull;           // 파일 있는지 없는지 여부 판별용 변수

    while (1) {
        memset(command, 0, sizeof(command)); // command 버퍼 비우기

        if (recv(client_sock, command, sizeof(command), 0) <= 0) { // recv 이상한 값 오는 경우
            printf("client disconnected.\n");
            break;
        }

        if (strcmp(command, "get") == 0) {         // 사용자 입장에서 다운로드
            memset(filename, 0, sizeof(filename)); // filename 버퍼 비우기

            if (recv(client_sock, filename, sizeof(filename), 0) <= 0) { // recv 에러 처리 1
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Requesting file [%s]\n", client_sock, filename); // 디버깅용...

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                perror("file open failed");
                isnull = 0;
                send(client_sock, &isnull, sizeof(isnull), 0); // 파일 열기 실패했다고 전송 2
                continue;
            }

            isnull = 1;
            send(client_sock, &isnull, sizeof(isnull), 0); // 파일 열기 성공했다고 전송 2

            /* 자, 여기서부터 잘 확인하기 */
            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(client_sock, &netFileSize, sizeof(netFileSize), 0); // 파일 사이즈 전송 3
            /* 여기까지 잘 작동하는가? */

            sentSize = 0; // 여기서 파일 전송 시작하는데 sentSize가 fileSize일 때 까지 전송함
            while (sentSize < fileSize) {
                recvSize = read(fd, buf, BUFSIZE);
                if (recvSize <= 0)
                    break;
                send(client_sock, buf, recvSize, 0); // 파일 전송 4
                sentSize += recvSize;
            }

            printf("File [%s] sent to client (%u bytes)\n", filename, fileSize);
            close(fd);

        } else if (strcmp(command, "put") == 0) {
            memset(filename, 0, sizeof(filename));

            if (recv(client_sock, filename, sizeof(filename), 0) <= 0) { // 파일 이름 받기 1
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Uploading file [%s]\n", client_sock, filename);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("file creation failed");
                break;
            }

            if (recv(client_sock, &netFileSize, sizeof(netFileSize), 0) <= 0) { // 파일 크기 수신 2
                printf("receiving file size failed\n");
                close(fd);
                break;
            }

            // 네트워크 바이트 순서를 호스트 바이트 순서로 변환
            fileSize = ntohl(netFileSize);
            printf("Receiving file [%s] (%u bytes)\n", filename, fileSize); // 파일 정보 출력

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = recv(client_sock, buf, BUFSIZE, 0); // 파일 순서대로 받기 3
                if (recvSize <= 0)
                    break;
                write(fd, buf, recvSize);
                sentSize += recvSize;
            }

            if (sentSize == fileSize) {
                printf("File [%s] received successfully.\n", filename);
            } else {
                printf("File [%s] transfer incomplete.\n", filename);
            }
            close(fd);

        } else {
            printf("Client(%d): Invalid command [%s]\n", client_sock, command);
        }
    }

    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in cliaddr;
    int listen_sock, *client_sock;
    socklen_t addrlen;
    pthread_t tid;

    listen_sock = tcp_listen(INADDR_ANY, 8080, 10);
    printf("Server listening on port 8080...\n");

    while (1) {
        addrlen = sizeof(cliaddr);
        client_sock = malloc(sizeof(int));

        *client_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);
        if (*client_sock < 0) {
            errquit("accept failed");
            free(client_sock);
            continue;
        }

        printf("Client connected (socket: %d)\n", *client_sock);

        if (pthread_create(&tid, NULL, client_handler, (void *)client_sock) != 0) {
            errquit("Thread creation failed");
            close(*client_sock);
            free(client_sock);
        } else {
            pthread_detach(tid);
        }
    }

    close(listen_sock);
    return 0;
}