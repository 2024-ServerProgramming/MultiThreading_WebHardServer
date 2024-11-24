#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 1024
#define THREAD_COUNT 4

typedef struct {
    int thread_id;
    int client_sock;
    const char *filename;
    size_t start, end;
} thread_data_t;

// 파일의 특정 범위를 서버에 전송
void *send_file_range(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char buf[BUFSIZE];
    int file_fd = open(data->filename, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return NULL;
    }

    lseek(file_fd, data->start, SEEK_SET);
    size_t bytes_left = data->end - data->start;

    while (bytes_left > 0) {
        size_t to_read = bytes_left > BUFSIZE ? BUFSIZE : bytes_left;
        ssize_t nbytes = read(file_fd, buf, to_read);
        if (nbytes <= 0)
            break;

        send(data->client_sock, buf, nbytes, 0);
        bytes_left -= nbytes;
    }

    close(file_fd);
    return NULL;
}

// 파일의 특정 범위를 서버로부터 수신
void *receive_file_range(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char buf[BUFSIZE];
    int file_fd = open(data->filename, O_WRONLY | O_CREAT, 0644);
    if (file_fd < 0) {
        perror("Failed to create file");
        return NULL;
    }

    lseek(file_fd, data->start, SEEK_SET);
    size_t bytes_left = data->end - data->start;

    while (bytes_left > 0) {
        size_t to_read = bytes_left > BUFSIZE ? BUFSIZE : bytes_left;
        ssize_t nbytes = recv(data->client_sock, buf, to_read, 0);
        if (nbytes <= 0)
            break;

        write(file_fd, buf, nbytes);
        bytes_left -= nbytes;
    }

    close(file_fd);
    return NULL;
}

void send_put(int client_sock, const char *filename) {
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return;
    }

    // 파일 크기 전송
    size_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);
    send(client_sock, &file_size, sizeof(size_t), 0);

    // 4개의 쓰레드로 파일 전송
    pthread_t threads[THREAD_COUNT];
    thread_data_t thread_data[THREAD_COUNT];
    size_t range_size = file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].client_sock = client_sock;
        thread_data[i].filename = filename;
        thread_data[i].start = i * range_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? file_size : (i + 1) * range_size;

        pthread_create(&threads[i], NULL, send_file_range, &thread_data[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[PUT] File '%s' uploaded to server.\n", filename);
}

void send_get(int client_sock, const char *filename) {
    // 서버에서 파일 크기 수신
    size_t file_size;
    recv(client_sock, &file_size, sizeof(size_t), 0);

    if (file_size == 0) {
        printf("[GET] File '%s' not found on server.\n", filename);
        return;
    }

    printf("[GET] Receiving file '%s' of size %zu bytes\n", filename, file_size);

    // 4개의 쓰레드로 파일 수신
    pthread_t threads[THREAD_COUNT];
    thread_data_t thread_data[THREAD_COUNT];
    size_t range_size = file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].client_sock = client_sock;
        thread_data[i].filename = filename;
        thread_data[i].start = i * range_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? file_size : (i + 1) * range_size;

        pthread_create(&threads[i], NULL, receive_file_range, &thread_data[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[GET] File '%s' downloaded from server.\n", filename);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    char command[BUFSIZE], filename[BUFSIZE];
    while (1) {
        printf("Enter command (put <filename>, get <filename>, quit): ");
        fgets(command, BUFSIZE, stdin);
        command[strcspn(command, "\n")] = '\0'; // Remove newline character

        if (strncmp(command, "put", 3) == 0) {
            send(client_sock, "put", 5, 0);
            sscanf(command + 4, "%s", filename);
            send(client_sock, filename, strlen(filename) + 1, 0);
            send_put(client_sock, filename);
        } else if (strncmp(command, "get", 3) == 0) {
            send(client_sock, "get", 5, 0);
            sscanf(command + 4, "%s", filename);
            send(client_sock, filename, strlen(filename) + 1, 0);
            send_get(client_sock, filename);
        } else if (strcmp(command, "quit") == 0) {
            send(client_sock, "quit", 5, 0);
            printf("Disconnecting...\n");
            break;
        } else {
            printf("Unknown command: %s\n", command);
        }
    }

    close(client_sock);
    return 0;
}
