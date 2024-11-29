#include "client_config.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void client_control(int sd) {
    while (1) {
        char command[10]; // 명령어 저장
        char filename[MAX_LENGTH];
        int fd;               // 파일 디스크립터
        unsigned sentSize;    // 파일 받은 사이즈 합
        unsigned recvSize;    // 파일 받은 사이즈
        unsigned fileSize;    // 총 파일 사이즈
        unsigned netFileSize; // 네트워크 전송용 파일 사이즈
        char buf[BUFSIZE];
        int isnull; // 파일 존재 여부 판별용 변수
        int success = 0;

        sleep(1);
        (void)system("clear");
        printf("\nEnter command (get/put/exit): ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "exit") == 0) {
            printf("exiting client.\n");
            break;
        }

        if (send(sd, command, strlen(command), 0) <= 0) {
            perror("send cmd failed...");
            break;
        }

        /* 다운로드 */
        if (strcmp(command, "get") == 0) {
            /*파일 이름 입력 부분*/
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            if (send(sd, filename, strlen(filename), 0) <= 0) {
                perror("send filename failed...");
                continue;
            }

            if (recv(sd, &isnull, sizeof(isnull), 0) <= 0) {
                perror("receiving file existence fail");
                continue;
            }

            if (isnull == 0) {
                printf("file not found on server.\n");
                continue;
            }

            if (recv(sd, &netFileSize, sizeof(netFileSize), 0) <= 0) {
                perror("receiving file size fail");
                continue;
            }

            fileSize = ntohl(netFileSize);
            printf("downloading file [%s] (%u bytes)\n", filename, fileSize);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("file open failed");
                continue;
            }

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = recv(sd, buf, BUFSIZE, 0);
                if (recvSize <= 0)
                    break;
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
            filename[strcspn(filename, "\n")] = '\0';

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                perror("file open failed");
                isnull = 0;
                continue;
            }

            if (send(sd, filename, strlen(filename), 0) <= 0) {
                perror("send filename failed...");
                close(fd);
                continue;
            }

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(sd, &netFileSize, sizeof(netFileSize), 0);

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize);

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = read(fd, buf, BUFSIZE);
                if (recvSize <= 0)
                    break;
                send(sd, buf, recvSize, 0);
                sentSize += recvSize;
            }

            if (sentSize == fileSize) {
                printf("file [%s] uploaded successfully.\n", filename);
            } else {
                printf("file [%s] upload incomplete.\n", filename);
            }
            close(fd);
        }
        /* 파일 삭제 */
        else if (strncmp(command, "delete", 6) == 0) {
            printf("삭제할 파일명 입력: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

            send(sd, filename, sizeof(filename), 0); // 서버에 파일명 전송

            recv(sd, &success, sizeof(success), 0);
            if (success)
                printf("파일 삭제 성공: %s\n", filename);
            else
                printf("파일 삭제 실패\n");
        } else {
            printf("invalid command. Use 'get', 'put', 'delete', 'show' or 'exit'.\n");
        }
    }

    close(sd);
}