#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define THREAD_COUNT 4

typedef struct {
    int sock;
    int fd;
    off_t start;
    off_t end;
} ThreadArgs;

// 서버에서 파일 데이터를 수신하고 파일에 기록하는 쓰레드 함수
void *receive_file_chunk(void *args) {
    ThreadArgs *targs = (ThreadArgs *)args;
    off_t remaining = targs->end - targs->start;
    char buffer[BUFFER_SIZE];

    lseek(targs->fd, targs->start, SEEK_SET);

    while (remaining > 0) {
        ssize_t bytes_to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        memset(buffer, 0, BUFFER_SIZE); // 버퍼 초기화
        ssize_t bytes_received = recv(targs->sock, buffer, bytes_to_read, 0);

        if (bytes_received <= 0)
            break;

        write(targs->fd, buffer, bytes_received);
        remaining -= bytes_received;
    }

    free(targs); // 동적 메모리 해제
    pthread_exit(NULL);
}

int main() {
    int sock;
    struct sockaddr_in server_addr;

    // 서버 주소 설정
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");

    // 서버에 요청할 파일 이름 전송
    char filename[256];
    printf("Enter the filename to download: ");
    scanf("%s", filename);
    send(sock, filename, strlen(filename) + 1, 0);

    // 서버로부터 파일 크기 수신
    off_t file_size;
    recv(sock, &file_size, sizeof(file_size), 0);
    printf("Downloading file '%s' (%ld bytes)...\n", filename, file_size);

    // 다운로드할 파일 생성
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("File creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 쓰레드를 이용하여 데이터 수신
    pthread_t threads[THREAD_COUNT];
    off_t chunk_size = file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        ThreadArgs *targs = malloc(sizeof(ThreadArgs));
        targs->sock = sock;
        targs->fd = fd;
        targs->start = i * chunk_size;
        targs->end = (i == THREAD_COUNT - 1) ? file_size : (i + 1) * chunk_size;

        pthread_create(&threads[i], NULL, receive_file_chunk, targs);
    }

    // 쓰레드 종료 대기
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("File '%s' downloaded successfully.\n", filename);
    close(fd);
    close(sock);
    return 0;
}
