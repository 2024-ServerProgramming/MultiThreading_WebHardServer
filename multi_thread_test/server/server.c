
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>

#define PORT 8080
#define BUFSIZE 1024
#define THREAD_COUNT 4
#define MAX_CLIENTS 10

pthread_mutex_t merge_mutex;

// 쓰레드 데이터 구조체
typedef struct {
    int thread_id;
    size_t start;
    size_t end;
    int file_fd;
    char *buffer;      // 각 쓰레드가 처리한 데이터를 저장
    size_t buffer_size;
} thread_data_t;

// 클라이언트 데이터 구조체
typedef struct {
    int client_id;
    int client_sock;
    char filename[256];
} client_data_t;

// 파일 범위를 처리하는 쓰레드 함수
void *process_file_range(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    size_t range_size = data->end - data->start;

    // 파일의 특정 범위 읽기
    lseek(data->file_fd, data->start, SEEK_SET);
    data->buffer = malloc(range_size);
    data->buffer_size = read(data->file_fd, data->buffer, range_size);

    printf("Thread %d processed range %zu-%zu (%zu bytes)\n",
           data->thread_id, data->start, data->end, data->buffer_size);

    return NULL;
}

// 클라이언트 요청을 처리하는 함수
void *handle_client(void *arg) {
    client_data_t *client_data = (client_data_t *)arg;
    char buf[BUFSIZE];
    ssize_t nbytes;

    printf("Client %d connected. Receiving file: %s\n", client_data->client_id, client_data->filename);

    // 클라이언트에서 파일 수신
    int file_fd = open(client_data->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("Failed to open file");
        close(client_data->client_sock);
        free(client_data);
        return NULL;
    }

    while ((nbytes = recv(client_data->client_sock, buf, BUFSIZE, 0)) > 0) {
        write(file_fd, buf, nbytes);
    }
    close(file_fd);

    // 파일 크기 확인
    file_fd = open(client_data->filename, O_RDONLY);
    size_t total_file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);
    printf("Received file size: %zu bytes\n", total_file_size);

    // 파일을 4개의 범위로 나눔
    pthread_t threads[THREAD_COUNT];
    thread_data_t thread_data[THREAD_COUNT];
    size_t range_size = total_file_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].start = i * range_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? total_file_size : (i + 1) * range_size;
        thread_data[i].file_fd = file_fd;

        pthread_create(&threads[i], NULL, process_file_range, &thread_data[i]);
    }

    // 쓰레드 종료 대기
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    close(file_fd);

    // 병합 파일 생성
    pthread_mutex_lock(&merge_mutex);
    int merge_fd = open("merged_file.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (merge_fd < 0) {
        perror("Failed to open merged file");
        pthread_mutex_unlock(&merge_mutex);
        free(client_data);
        return NULL;
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        write(merge_fd, thread_data[i].buffer, thread_data[i].buffer_size);
        free(thread_data[i].buffer);
    }
    close(merge_fd);
    pthread_mutex_unlock(&merge_mutex);

    printf("Client %d file merged successfully.\n", client_data->client_id);

    close(client_data->client_sock);
    free(client_data);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t client_threads[MAX_CLIENTS];
    int client_count = 0;

    pthread_mutex_init(&merge_mutex, NULL);

    // 서버 소켓 생성
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

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

    printf("Server is listening on port %d...\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) > 0) {
        client_data_t *client_data = malloc(sizeof(client_data_t));
        client_data->client_id = client_count;
        client_data->client_sock = client_sock;

        // 파일명 수신
        recv(client_sock, client_data->filename, sizeof(client_data->filename), 0);

        // 클라이언트 처리 쓰레드 생성
        pthread_create(&client_threads[client_count++], NULL, handle_client, client_data);

        if (client_count >= MAX_CLIENTS) {
            printf("Max clients reached. Waiting for threads to finish.\n");
            for (int i = 0; i < client_count; i++) {
                pthread_join(client_threads[i], NULL);
            }
            client_count = 0;
        }
    }

    close(server_sock);
    pthread_mutex_destroy(&merge_mutex);

    return 0;
}
