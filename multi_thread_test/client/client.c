#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFSIZE 1024

void errquit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s [server_ip] [port] [file_to_upload]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];

    // 소켓 생성
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) errquit("Socket creation failed");

    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        errquit("Invalid server IP address");
    }

    // 서버 연결
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        errquit("Connection to server failed");
    }
    printf("Connected to server %s:%d\n", server_ip, port);

    // 파일 이름 전송
    if (send(sock, filename, strlen(filename) + 1, 0) < 0) {
        errquit("Failed to send filename");
    }
    printf("Sent filename: %s\n", filename);

    // 파일 열기
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 파일 내용 전송
    char buf[BUFSIZE];
    ssize_t nbytes;
    while ((nbytes = read(file_fd, buf, BUFSIZE)) > 0) {
        if (send(sock, buf, nbytes, 0) < 0) {
            perror("Failed to send file data");
            close(file_fd);
            close(sock);
            exit(EXIT_FAILURE);
        }
    }
    close(file_fd);

    printf("File upload complete.\n");

    // 서버로부터 응답 대기 (선택 사항)
    char response[BUFSIZE];
    nbytes = recv(sock, response, BUFSIZE - 1, 0);
    if (nbytes > 0) {
        response[nbytes] = '\0';
        printf("Server response: %s\n", response);
    } else {
        printf("No response from server.\n");
    }

    close(sock);
    return 0;
}
