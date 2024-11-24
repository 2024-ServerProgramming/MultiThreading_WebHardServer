#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define MAXLINE 512
#define BUFSIZE 256

void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

int tcp_listen(int host, int port, int backlog) {
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) errquit("Socket creation failed");

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

    char command[6];  // 명령어 크기 확장
    char filename[MAXLINE];
    char buf[BUFSIZE];
    int fd, fileSize, sentSize, recvSize;
    unsigned int netFileSize;
    int isnull;

    while (1) {
        memset(command, 0, sizeof(command));
        if (recv(client_sock, command, sizeof(command), 0) <= 0) {
            printf("client disconnected.\n");
            break;
        }

        if (strcmp(command, "get") == 0) {
            memset(filename, 0, sizeof(filename));
            if (recv(client_sock, filename, sizeof(filename), 0) <= 0) {
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Requesting file [%s]\n", client_sock, filename);

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                errquit("file open failed");
                isnull = 0;
                send(client_sock, &isnull, sizeof(isnull), 0);
                continue;
            }

            isnull = 1;
            send(client_sock, &isnull, sizeof(isnull), 0);

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(client_sock, &netFileSize, sizeof(netFileSize), 0);

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = read(fd, buf, BUFSIZE);
                if (recvSize <= 0) break;
                send(client_sock, buf, recvSize, 0);
                sentSize += recvSize;
            }

            printf("File [%s] sent to client.\n", filename);
            close(fd);

        } else if (strcmp(command, "put") == 0) {
            memset(filename, 0, sizeof(filename));
            if (recv(client_sock, filename, sizeof(filename), 0) <= 0) {
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Uploading file [%s]\n", client_sock, filename);

            recv(client_sock, &isnull, sizeof(isnull), 0);
            if (isnull == 0) {
                printf("Client reported file open failure.\n");
                continue;
            }

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                errquit("file creation failed");
                break;
            }

            if (recv(client_sock, &netFileSize, sizeof(netFileSize), 0) <= 0) {
                printf("receiving file size failed\n");
                close(fd);
                break;
            }

            // 네트워크 바이트 순서를 호스트 바이트 순서로 변환
            fileSize = ntohl(netFileSize);
            printf("Receiving file [%s] (%d bytes)\n", filename, fileSize);

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = recv(client_sock, buf, BUFSIZE, 0);
                if (recvSize <= 0) break;
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
