#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 1024
#define THREAD_COUNT 4
#define MAX_CLIENTS 5

pthread_mutex_t file_mutex;

typedef struct {
    int client_sock;
    size_t start, end;
    int file_fd;
    char *buffer;
} thread_data_t;

typedef struct {
    int client_id;
    int client_sock;
} client_data_t;

// 서버가 파일 범위를 수신
void *receive_file_range(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char buf[BUFSIZE];
    size_t bytes_left = data->end - data->start;

    while (bytes_left > 0) {
        size_t to_read = bytes_left > BUFSIZE ? BUFSIZE : bytes_left;
        ssize_t nbytes = recv(data->client_sock, buf, to_read, 0);
        if (nbytes <= 0)
            break;

        // 파일 쓰기 작업 보호
        pthread_mutex_lock(&file_mutex);
        lseek(data->file_fd, data->start, SEEK_SET); // 지정된 위치로 파일 포인터 이동
        write(data->file_fd, buf, nbytes);           // 데이터 쓰기
        data->start += nbytes;                       // 시작 위치 갱신
        pthread_mutex_unlock(&file_mutex);

        bytes_left -= nbytes;
    }
    return NULL;
}

// 서버가 파일 범위를 전송
void *send_file_range(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char buf[BUFSIZE];
    size_t bytes_left = data->end - data->start;

    while (bytes_left > 0) {
        size_t to_read = bytes_left > BUFSIZE ? BUFSIZE : bytes_left;

        pthread_mutex_lock(&file_mutex);
        lseek(data->file_fd, data->start, SEEK_SET);        // 지정된 위치로 파일 포인터 이동
        ssize_t nbytes = read(data->file_fd, buf, to_read); // 파일 읽기
        pthread_mutex_unlock(&file_mutex);

        if (nbytes <= 0)
            break;
        send(data->client_sock, buf, nbytes, 0); // 클라이언트로 데이터 전송

        data->start += nbytes; // 시작 위치 갱신
        bytes_left -= nbytes;
    }
    return NULL;
}

void handle_put(int client_sock, const char *filename) {
    size_t total_file_size;
    recv(client_sock, &total_file_size, sizeof(size_t), 0);

    printf("[PUT] Receiving file '%s' of size %zu bytes\n", filename, total_file_size);

    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("Failed to create file");
        return;
    }

    pthread_t threads[THREAD_COUNT];
    thread_data_t thread_data[THREAD_COUNT];
    size_t range_size = total_file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].client_sock = client_sock;
        thread_data[i].start = i * range_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? total_file_size : (i + 1) * range_size;
        thread_data[i].file_fd = file_fd;

        pthread_create(&threads[i], NULL, receive_file_range, &thread_data[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    close(file_fd);
    printf("[PUT] File '%s' received and saved.\n", filename);
}

void handle_get(int client_sock, const char *filename) {
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        size_t zero = 0;
        send(client_sock, &zero, sizeof(size_t), 0); // 파일 크기를 0으로 전송
        return;
    }

    size_t total_file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);
    send(client_sock, &total_file_size, sizeof(size_t), 0);

    printf("[GET] Sending file '%s' of size %zu bytes\n", filename, total_file_size);

    pthread_t threads[THREAD_COUNT];
    thread_data_t thread_data[THREAD_COUNT];
    size_t range_size = total_file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].client_sock = client_sock;
        thread_data[i].start = i * range_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? total_file_size : (i + 1) * range_size;
        thread_data[i].file_fd = file_fd;

        pthread_create(&threads[i], NULL, send_file_range, &thread_data[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    close(file_fd);
    printf("[GET] File '%s' sent to client.\n", filename);
}

void *handle_client(void *arg) {
    client_data_t *client_data = (client_data_t *)arg;
    char command[BUFSIZE], filename[BUFSIZE];

    printf("[Client %d] Connected.\n", client_data->client_id);

    while (1) {
        recv(client_data->client_sock, command, sizeof(command), 0);

        if (strncmp(command, "put", 3) == 0) {
            recv(client_data->client_sock, filename, sizeof(filename), 0);
            handle_put(client_data->client_sock, filename);
        } else if (strncmp(command, "get", 3) == 0) {
            recv(client_data->client_sock, filename, sizeof(filename), 0);
            handle_get(client_data->client_sock, filename);
        } else if (strncmp(command, "quit", 4) == 0) {
            printf("[Client %d] Disconnected.\n", client_data->client_id);
            close(client_data->client_sock);
            free(client_data);
            return NULL;
        } else {
            printf("[Client %d] Unknown command: %s\n", client_data->client_id, command);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t client_threads[MAX_CLIENTS];
    int client_count = 0;

    int port = atoi(argv[1]);
    pthread_mutex_init(&file_mutex, NULL);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) > 0) {
        client_data_t *client_data = malloc(sizeof(client_data_t));
        client_data->client_id = client_count;
        client_data->client_sock = client_sock;

        pthread_create(&client_threads[client_count++], NULL, handle_client, client_data);

        if (client_count >= MAX_CLIENTS) {
            for (int i = 0; i < client_count; i++) {
                pthread_join(client_threads[i], NULL);
            }
            client_count = 0;
        }
    }

    close(server_sock);
    pthread_mutex_destroy(&file_mutex);
    return 0;
}
