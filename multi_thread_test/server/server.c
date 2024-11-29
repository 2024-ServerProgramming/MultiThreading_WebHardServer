#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define THREAD_COUNT 4

typedef struct {
    int client_sock;
    int fd;
    off_t start;
    off_t end;
} ThreadArgs;

// 파일 데이터를 클라이언트로 전송하는 쓰레드 함수
void *send_file_chunk(void *args) {
    ThreadArgs *targs = (ThreadArgs *)args;
    off_t remaining = targs->end - targs->start;
    char buffer[BUFFER_SIZE];

    // 파일 시작 위치로 이동
    lseek(targs->fd, targs->start, SEEK_SET);

    while (remaining > 0) {
        ssize_t bytes_to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        memset(buffer, 0, BUFFER_SIZE); // 버퍼 초기화
        ssize_t bytes_read = read(targs->fd, buffer, bytes_to_read);

        if (bytes_read <= 0)
            break;

        send(targs->client_sock, buffer, bytes_read, 0);
        remaining -= bytes_read;
    }

    free(targs); // 동적 메모리 해제
    pthread_exit(NULL);
}

// 각 클라이언트를 처리하는 쓰레드 함수
void *handle_client(void *args) {
    int client_sock = *(int *)args;
    free(args);

    // 클라이언트로부터 파일 요청 수신
    char filename[256];
    memset(filename, 0, sizeof(filename));
    recv(client_sock, filename, sizeof(filename), 0);

    printf("Client requested file: %s\n", filename);

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("File open failed");
        close(client_sock);
        pthread_exit(NULL);
    }

    // 파일 크기 계산
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // 파일 크기를 클라이언트에 전송
    send(client_sock, &file_size, sizeof(file_size), 0);

    // 파일을 4개로 분할하여 전송
    pthread_t threads[THREAD_COUNT];
    off_t chunk_size = file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        ThreadArgs *targs = malloc(sizeof(ThreadArgs));
        targs->client_sock = client_sock;
        targs->fd = fd;
        targs->start = i * chunk_size;
        targs->end = (i == THREAD_COUNT - 1) ? file_size : (i + 1) * chunk_size;

        pthread_create(&threads[i], NULL, send_file_chunk, targs);
    }

    // 모든 쓰레드 종료 대기
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("File '%s' sent successfully.\n", filename);
    close(fd);
    close(client_sock);
    pthread_exit(NULL);
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 서버 소켓 생성
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port 8080...\n");

    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("New client connected.\n");

        // 클라이언트를 처리할 쓰레드 생성
        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_sock;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_sock_ptr);
        pthread_detach(thread); // 쓰레드 종료 시 자원 자동 해제
    }

    close(server_sock);
    return 0;
}
