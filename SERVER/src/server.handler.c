#include "server.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void *process_range(void *off) {
    OFFIN *off_info = (OFFIN *)off;

    int fd = open(off_info->filename, O_RDONLY);
    if (fd < 0) {
        perror("File open failed");
        pthread_exit(NULL);
    }

    lseek(fd, off_info->start, SEEK_SET);
    off_t remaining = off_info->end - off_info->start;
    char buffer[1024];

    while (remaining > 0) {
        ssize_t bytes_to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
        ssize_t bytes_read = read(fd, buffer, bytes_to_read);
        if (bytes_read <= 0) {
            perror("File read failed");
            break;
        }

        off_t offset = off_info->start + (off_info->end - remaining);
        if (send(off_info->client_sock, &offset, sizeof(offset), 0) <= 0) {
            perror("Send offset failed");
            break;
        }

        if (send(off_info->client_sock, &bytes_read, sizeof(bytes_read), 0) <= 0) {
            perror("Send data length failed");
            break;
        }

        if (send(off_info->client_sock, buffer, bytes_read, 0) <= 0) {
            perror("Send data failed");
            break;
        }

        remaining -= bytes_read;
    }

    close(fd);
    free(off_info); // 메모리 해제
    pthread_exit(NULL);
}

void *client_handle(CliSession *cliS) {
    char command[10]; // 명령어 저장
    char filename[MAX_LENGTH];
    char buf[BUFSIZE];
    int fd;                // 파일 디스크립터
    unsigned fileSize;     // 파일 송수신할 때 총 파일 사이즈
    unsigned sentSize = 0; // 파일 보낸 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
    unsigned recvSize;     // 파일 받은 사이즈
    unsigned netFileSize;  // size_t == unsigned, 네트워크 전송용
    int isnull;            // 파일 있는지 없는지 여부 판별용 변수
    int success = 0;

    find_session(cliS->session->session_id);

    while (1) {
        memset(command, 0, sizeof(command));

        if (recv(cliS->cli_data, command, sizeof(command), 0) <= 0) {
            printf("client disconnected.\n");
            break;
        }

        /* 파일 다운로드 명령어 */
        if (strcmp(command, "get") == 0) {
            memset(filename, 0, sizeof(filename));

            if (recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0) {
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Requesting file [%s]\n", cliS->cli_data, filename);

            /* 사용자 디렉토리에서 파일 열기 */
            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            fd = open(filepath, O_RDONLY);
            if (fd < 0) {
                perror("file open failed");
                isnull = 0;
                send(cliS->cli_data, &isnull, sizeof(isnull), 0);
                continue;
            }

            isnull = 1;
            send(cliS->cli_data, &isnull, sizeof(isnull), 0);

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(cliS->cli_data, &netFileSize, sizeof(netFileSize), 0);

            pthread_t pthreads[4];
            printf("Sending file [%s] (%u bytes)\n", filename, fileSize);

            for (int i = 0; i < 4; i++) {
                OFFIN *off = malloc(sizeof(OFFIN));
                if (!off) {
                    perror("Memory allocation failed");
                    close(fd);
                    break;
                }

                off->start = (fileSize / 4) * i;
                off->end = (i == 3) ? fileSize : (fileSize / 4) * (i + 1);
                off->fd = fd;
                off->client_sock = cliS->cli_data;
                snprintf(off->filename, sizeof(off->filename), "./user_data/%s/%s", cliS->session->user_id, filename);

                if (pthread_create(&pthreads[i], NULL, process_range, off) != 0) {
                    perror("Thread creation failed");
                    free(off);
                    continue;
                }
            }

            for (int i = 0; i < 4; i++) {
                pthread_join(pthreads[i], NULL);
            }

            printf("File [%s] sent to client (%u bytes).\n", filename, fileSize);
            close(fd);
        }
        /* put 명령어 */
        else if (strcmp(command, "put") == 0) {
            memset(filename, 0, sizeof(filename));

            if (recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0) {
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Uploading file [%s]\n", cliS->cli_data, filename);

            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("file creation failed");
                break;
            }

            if (recv(cliS->cli_data, &netFileSize, sizeof(netFileSize), 0) <= 0) {
                printf("receiving file size failed\n");
                close(fd);
                break;
            }

            // 네트워크 바이트 순서를 호스트 바이트 순서로 변환
            fileSize = ntohl(netFileSize);
            printf("Receiving file [%s] (%u bytes)\n", filename, fileSize);

            sentSize = 0;
            while (sentSize < fileSize) {
                recvSize = recv(cliS->cli_data, buf, BUFSIZE, 0);
                if (recvSize <= 0)
                    break;
                if (write(fd, buf, recvSize) < 0) {
                    perror("Write failed");
                    break;
                }
                sentSize += recvSize;
            }

            if (sentSize == fileSize) {
                printf("File [%s] received successfully.\n", filename);
            } else {
                printf("File [%s] transfer incomplete.\n", filename);
            }
            close(fd);
        } else if (strcmp(command, "show") == 0) {
            FILE *fp;
            char result[BUFSIZE];

            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "ls -a ./user_data/%s", cliS->session->user_id);

            fp = popen(filepath, "r");
            if (fp == NULL) {
                perror("Failed to run command");
                continue;
            }

            // 명령어 실행 결과를 읽어와 클라이언트로 전송
            while (fgets(result, sizeof(result), fp) != NULL) {
                if (send(cliS->cli_data, result, strlen(result), 0) == -1) {
                    perror("Failed to send");
                    break;
                }
            }

            // 명령어 실행 종료
            pclose(fp);

            strcpy(result, "FILE_END");
            send(cliS->cli_data, result, strlen(result), 0);
        }
        // 파일 삭제 처리
        else if (strcmp(command, "delete") == 0) {
            memset(filename, 0, sizeof(filename));

            int n = recv(cliS->cli_data, filename, sizeof(filename) - 1, 0);
            if (n <= 0) {
                printf("receiving filename failed\n");
                break;
            }
            filename[n] = '\0';

            printf("Client(%d): Deleting file [%s]\n", cliS->cli_data, filename);

            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            if (remove(filepath) == 0) {
                success = 1;
                printf("File [%s] deleted successfully.\n", filepath);
            } else {
                success = 0;
                perror("File delete failed");
            }

            send(cliS->cli_data, &success, sizeof(int), 0);
        } else {
            printf("Client(%d): Invalid command [%s]\n", cliS->cli_data, command);
        }
        update_session(cliS->session);
    }

    close(cliS->cli_data);
    return NULL;
}