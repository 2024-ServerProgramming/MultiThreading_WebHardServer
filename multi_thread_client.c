#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#define MAXLINE 512
#define BUFSIZE 256

#define PORT_NUM 9876

void errquit(const char *mesg) {
    perror(mesg);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("./%s <server ip> 입력하시오...\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int sock;
    struct sockaddr_in servaddr;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) errquit("Socket creation failed");

    // 서버 주소 설정
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT_NUM);
    if(inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        errquit("invalid address\n");
    }

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errquit("connect failed...");
    }
    printf("Connected to server at %s:%d\n", server_ip, PORT_NUM);

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

        printf("\nEnter command (get/put/tree/rm/exit): ");
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

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = recv(sock, buf, BUFSIZE, 0); // 파일 전송 받기 4
                if (recvSize <= 0) break;
                write(fd, buf, recvSize);
                sentSize += recvSize;
            }

            if (sentSize == fileSize) {
                printf("file [%s] downloaded successfully.\n", filename);
            } else {
                printf("file [%s] download incomplete.\n", filename);
            }
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
        } else if (strcmp(command, "tree") == 0) {
            printf("서버 파일 구조:\n");
            while (1) {
                int n = recv(sock, buf, BUFSIZE, 0);
                if (n <= 0) break;
                buf[n] = '\0';
                printf("%s", buf);
            }
        } else {
            printf("invalid command. Use 'get', 'put', 'tree', 'rm', or 'exit'.\n");
        }
    }

    close(sock);
    return 0;
}
