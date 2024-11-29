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

pthread_mutex_t socket_lock; // 소켓 접근을 위한 뮤텍스

typedef struct offset_info {
    int fd;            // 파일 디스크립터
    int client_sock;   // 클라이언트 소켓
    off_t start;       // 전송 시작 위치
    off_t end;         // 전송 종료 위치
    char buffer[1024]; // 데이터 버퍼
} OFFIN;

void What_I_received(OFFIN off) {
    printf("전송할 크기 : %d\n", sizeof(off));
    printf("전송할 fd : %d\n", off.fd);
    printf("전송할 start : %d\n", off.start);
    printf("전송할 end : %d\n", off.end);
    printf("전송할 client_sock : %d\n", off.client_sock);
    printf("전송할 buffer : %s\n", off.buffer);
}

// 파일의 특정 범위를 처리하는 함수 (멀티스레드)
void *process_range(void *off) {

    OFFIN *off_info = (OFFIN *)off;
    What_I_received(*off_info);
    // 파일의 시작 위치로 파일 포인터 이동
    lseek(off_info->fd, off_info->start, SEEK_SET);

    off_t remaining = off_info->end - off_info->start; // 남은 데이터 크기 계산
    int first_flag = 1;                                // 첫 패킷인지 여부

    while (remaining > 0) {
        // 현재 읽어야 할 바이트 계산
        ssize_t byte_to_read = (remaining < 1024) ? remaining : 1024;
        ssize_t bytes_read = read(off_info->fd, off_info->buffer, byte_to_read); // 파일에서 읽기

        if (first_flag) {
            // 첫 번째 패킷: start는 고정, end는 현재 읽은 크기
            off_info->end = off_info->start + bytes_read;
            first_flag = 0;
        } else {
            // 두 번째 이후 패킷: start는 이전 end, end는 현재 end + 읽은 크기
            off_info->start = off_info->end;
            off_info->end += bytes_read;
        }

        // 전송할 데이터 정보 설정
        OFFIN send_info = {
            .fd = off_info->fd,
            .client_sock = off_info->client_sock,
            .start = off_info->start,
            .end = off_info->end,
        };
        memcpy(send_info.buffer, off_info->buffer, bytes_read);

        // 동기화된 소켓 접근을 위해 뮤텍스 사용
        pthread_mutex_lock(&socket_lock);
        char ack[4];
        strcpy(ack, "ACK");
        
        send(off_info->client_sock, &send_info, sizeof(send_info), 0); // 데이터 전송
        recv(off_info->client_sock, ack, sizeof(ack), 0);

        remaining -= bytes_read; // 남은 데이터 크기 갱신
    }
    return NULL;
}

// 오류 출력 및 종료 함수
void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

// TCP 서버 초기화 함수
int tcp_listen(int host, int port, int backlog) {
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);

    if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("bind failed");
    }

    if (listen(sd, backlog) < 0) {
        errquit("listen failed");
    }

    return sd;
}

// 클라이언트 요청 처리 스레드
void *client_handler(void *input) {
    int client_sock = *(int *)input;
    free(input);

    char command[5];        // 명령어
    char filename[MAXLINE]; // 파일 이름
    char buf[BUFSIZE];      // 버퍼
    int fd;                 // 파일 디스크립터
    unsigned fileSize;      // 파일 크기
    unsigned sentSize = 0;  // 전송된 데이터 크기
    unsigned netFileSize;   // 네트워크 바이트 순서의 파일 크기
    unsigned recvSize = 0;  // 파일 받은 사이즈
    int isnull;             // 파일 존재 여부

    while (1) {
        memset(command, 0, sizeof(command)); // 명령어 버퍼 초기화

        // 클라이언트로부터 명령어 수신
        if (recv(client_sock, command, sizeof(command), 0) <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        if (strcmp(command, "get") == 0) { // 파일 다운로드 요청
            memset(filename, 0, sizeof(filename));
            if (recv(client_sock, filename, sizeof(filename), 0) <= 0) {
                printf("Failed to receive filename.\n");
                break;
            }

            printf("Client(%d): Requesting file [%s]\n", client_sock, filename);

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                perror("File open failed");
                isnull = 1;
                send(client_sock, &isnull, sizeof(isnull), 0);
                continue;
            }

            isnull = 0;
            send(client_sock, &isnull, sizeof(isnull), 0);

            // 파일 크기 계산
            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(client_sock, &netFileSize, sizeof(netFileSize), 0);

            // 파일 전송 작업을 위한 스레드 생성
            OFFIN off[4];
            pthread_t pthreads[4];

            for (int i = 0; i < 4; i++) {
                off[i].start = (fileSize / 4) * i;
                off[i].end = (i == 3) ? fileSize : (fileSize / 4) * (i + 1);
                off[i].fd = fd;
                off[i].client_sock = client_sock;
            }

            for (int i = 0; i < 4; i++) {
                pthread_create(&pthreads[i], NULL, process_range, &off[i]);
            }

            for (int i = 0; i < 4; i++) {
                pthread_join(pthreads[i], NULL);
            }

            printf("File [%s] sent to client (%u bytes).\n", filename, fileSize);
            close(fd);

        } else if (strcmp(command, "put") == 0) {
            memset(filename, 0, sizeof(filename));

            if (recv(client_sock, filename, sizeof(filename), 0) <= 0) {
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
            printf("Invalid command: %s\n", command);
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

    // 뮤텍스 초기화
    pthread_mutex_init(&socket_lock, NULL);

    // TCP 서버 초기화
    listen_sock = tcp_listen(INADDR_ANY, 8080, 10);
    printf("Server listening on port 8080...\n");

    while (1) {
        addrlen = sizeof(cliaddr);
        client_sock = malloc(sizeof(int));

        *client_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);
        if (*client_sock < 0) {
            perror("Accept failed");
            free(client_sock);
            continue;
        }

        printf("Client connected (socket: %d)\n", *client_sock);

        if (pthread_create(&tid, NULL, client_handler, client_sock) != 0) {
            perror("Thread creation failed");
            close(*client_sock);
            free(client_sock);
        } else {
            pthread_detach(tid);
        }
    }

    // 리소스 정리
    pthread_mutex_destroy(&socket_lock);
    close(listen_sock);
    return 0;
}
