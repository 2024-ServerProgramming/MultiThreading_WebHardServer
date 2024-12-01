#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define MAXLINE 512
#define BUFSIZE 2048

// pthread_mutex_t file_mutex;
// pthread_mutex_t socket_mutex;

void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

// 나야 그 긴거
typedef struct {
    int sock;
    int fd;
    unsigned start_offset;
    unsigned end_offset;
} RecvInfo;

void *recv_handler(void *input) {
    RecvInfo *data = (RecvInfo *)input;
    char buf[BUFSIZE];
    unsigned offset = data->start_offset;

    // pwrite를 사용하여 파일 동기화 문제 해결
    while (offset < data->end_offset) {
        unsigned chunk_size = (data->end_offset - offset > BUFSIZE) ? BUFSIZE : (data->end_offset - offset);
        int recv_byte = recv(data->sock, buf, chunk_size, 0);
        if (recv_byte <= 0) break;

        if (pwrite(data->fd, buf, recv_byte, offset) <= 0) {
            perror("write failed");
            break;
        }

        offset += recv_byte;
    }

    free(data);
    return NULL;
}
// 뇌가 녹는다...

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("./%s <server ip> 입력하시오...\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int sock;
    struct sockaddr_in servaddr;

    // pthread_mutex_init(&file_mutex, NULL);
    // pthread_mutex_init(&socket_mutex, NULL);

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) errquit("Socket creation failed");

    // 서버 주소 설정
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    if(inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        errquit("invalid address\n");
    }

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("connect failed...");
    }
    printf("Connected to server at %s:8080\n", server_ip);

    while (1) {
        char command[5]; // 명령어 저장
        char filename[MAXLINE];
        int fd;           // 파일 디스크립터 
        unsigned sentSize;     // 파일 받은 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
        unsigned recvSize;     // 파일 받은 사이즈
        unsigned fileSize;     // 총 파일 사이즈
        unsigned netFileSize; // size_t == unsined 총 파일 사이즈, 네트워크 전송용
        char buf[BUFSIZE];
        int isnull;       // 파일 있는지 없는지 여부 판별용 변수

        printf("\nEnter command (get/put/exit): ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        // 종료 명령 처리
        if (strcmp(command, "exit") == 0) {
            printf("exiting client.\n");
            break;
        }

        // 서버로 명령어 전송
        if (send(sock, command, strlen(command), 0) <= 0) {
            errquit("send cmd failed...");
            break;
        }

        if (strcmp(command, "get") == 0) { // 다운로드
            /*파일 이름 입력 부분*/        
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0'; // 아... 이런게 있네
            /* 여기까지 */

            send(sock, filename, strlen(filename), 0); // 파일 이름 전송 1

            if (recv(sock, &isnull, sizeof(isnull), 0) <= 0) { // 파일 이름 있는지 없는지 2
                perror("receiving file existence fail");
                continue;
            }

            if (isnull == 0) {
                printf("file not found on server.\n");
                continue;
            }

            if (recv(sock, &netFileSize, sizeof(netFileSize), 0) <= 0) { // 전달 받은 파일 사이즈 3
                perror("receiving file size fail");
                continue;
            }

            fileSize = ntohl(netFileSize);
            printf("downloading file [%s] (%u bytes)\n", filename, fileSize);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                errquit("file open failed");
                continue;
            }

            /* 여기서부터 받는거 !!! 함 해보자 */
            unsigned threadCnt = (fileSize + BUFSIZE - 1) / BUFSIZE;
            pthread_t *thread = malloc(threadCnt * sizeof(pthread_t));
            
            for(int i = 0; i < threadCnt; i++) {
                RecvInfo *info = malloc(sizeof(RecvInfo));
                info->sock = sock;
                info->fd = fd;
                info->start_offset = i * BUFSIZE;
                info->end_offset = (i == threadCnt - 1) ? fileSize : (i + 1) * BUFSIZE;

                pthread_create(&thread[i], NULL, recv_handler, info);
            }

            for(int i = 0; i <threadCnt; i++)
                pthread_join(thread[i], NULL);

            free(thread);
            
            printf("file [%s] downloaded successfully.\n", filename);
            close(fd);

        } else if (strcmp(command, "put") == 0) {
            memset(filename, 0, sizeof(filename));

            /* 업로드할 파일 이름 입력 */
            printf("enter filename to upload: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0'; // 이건 신이야...
            /* 여기까지 */

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                perror("file open failed");
                isnull = 0;
                continue;
            }

            send(sock, filename, strlen(filename), 0); // 파일 이름 전송 1

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(sock, &netFileSize, sizeof(netFileSize), 0); // 파일 크기 송신 2

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize); // 업로드할 파일 정보 출력

            int sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = read(fd, buf, BUFSIZE);
                if (recvSize <= 0) break;
                send(sock, buf, recvSize, 0); // 파일 순서대로 보내기 3
                sentSize += recvSize;
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
