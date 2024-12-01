#include "server.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void *send_handler(void *input){
    SendInfo *data = (SendInfo *)input;
    char buf[BUF_SIZE];
    unsigned offset = data->start_offset;

    // pread를 사용하여 파일 동기화 문제 해결
    while(offset < data->end_offset){
        unsigned chunk_size = (data->end_offset - offset > BUF_SIZE) ? BUF_SIZE : (data->end_offset - offset);
        int read_byte = pread(data->file_fd, buf, chunk_size, offset);
        if (read_byte <= 0) break;

        // 소켓 전송
        if(send(data->client_sock, buf, read_byte, 0) <= 0){
            perror("send failed");
            break;
        }

        offset += read_byte;
    }

    free(data);
    return NULL;
}

void *client_handle(CliSession *cliS){
    char command[10]; // 명령어 저장
    char filename[MAXLENGTH];
    char buf[BUFSIZE];
    int fd;                // 파일 디스크립터
    unsigned fileSize;     // 파일 송수신할 때 총 파일 사이즈
    unsigned sentSize = 0; // 파일 보낸 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
    unsigned recvSize;     // 파일 받은 사이즈
    unsigned netFileSize;  // size_t == unsigned, 네트워크 전송용
    int isnull;            // 파일 있는지 없는지 여부 판별용 변수
    int success = 0;

    find_session(cliS->session->session_id);

    while(1){
        memset(command, 0, sizeof(command));

        if(recv(cliS->cli_data, command, sizeof(command), 0) <= 0){
            printf("client disconnected.\n");
            break;
        }

        /* 파일 다운로드 명령어 */
        if(strcmp(command, "get") == 0){
            memset(filename, 0, sizeof(filename));

            if(recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0){
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Requesting file [%s]\n", cliS->cli_data, filename);

            /* 사용자 디렉토리에서 파일 열기 */
            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            fd = open(filepath, O_RDONLY);
            if(fd < 0){
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

            // 파일을 2048 바이트씩 나누어 여러 쓰레드로 전송
            unsigned ThreadCnt = (fileSize + BUF_SIZE - 1) / BUF_SIZE;
            pthread_t *threads = malloc(ThreadCnt * sizeof(pthread_t));
            for(unsigned i = 0; i < ThreadCnt; i++){
                SendInfo *info = malloc(sizeof(SendInfo));
                if(!info){
                    perror("Memory allocation failed");
                    close(fd);
                    break;
                }
                info->client_sock = cliS->cli_data;
                info->file_fd = fd;
                info->start_offset = i * BUF_SIZE;
                info->end_offset = (i == ThreadCnt - 1) ? fileSize : (i + 1) * BUF_SIZE;
                snprintf(info->filename, sizeof(info->filename), "./user_data/%s/%s", cliS->session->user_id, filename);

                if(pthread_create(&threads[i], NULL, send_handler, info) != 0){
                    perror("Thread creation failed");
                    free(info);
                    continue;
                }
            }

            for(unsigned i = 0; i < ThreadCnt; i++){
                pthread_join(threads[i], NULL);
            }

            free(threads);
            printf("File [%s] sent to client (%u bytes).\n", filename, fileSize);
            close(fd);
        }
        /* put 명령어 */
        else if(strcmp(command, "put") == 0){
            memset(filename, 0, sizeof(filename));

            if(recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0){
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Uploading file [%s]\n", cliS->cli_data, filename);

            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0){
                perror("file creation failed");
                break;
            }

            if(recv(cliS->cli_data, &netFileSize, sizeof(netFileSize), 0) <= 0){
                printf("receiving file size failed\n");
                close(fd);
                break;
            }

            // 네트워크 바이트 순서를 호스트 바이트 순서로 변환
            fileSize = ntohl(netFileSize);
            printf("Receiving file [%s] (%u bytes)\n", filename, fileSize);

            sentSize = 0;
            while(sentSize < fileSize){
                recvSize = recv(cliS->cli_data, buf, BUF_SIZE, 0);
                if (recvSize <= 0)
                    break;
                ssize_t bytes_written = pwrite(fd, buf, recvSize, sentSize); // pwrite 사용
                if (bytes_written < 0) {
                    perror("Write failed");
                    break;
                }
                sentSize += recvSize;
            }

            if(sentSize == fileSize){
                printf("File [%s] received successfully.\n", filename);
            } else {
                printf("File [%s] transfer incomplete.\n", filename);
            }
            close(fd);
        } 
        else if (strcmp(command, "show") == 0){
            FILE *fp;
            char result[BUF_SIZE];

            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "ls -a ./user_data/%s", cliS->session->user_id);

            fp = popen(filepath, "r");
            if (fp == NULL) {
                perror("Failed to run command");
                continue;
            }

            // 명령어 실행 결과를 읽어와 클라이언트로 전송
            while (fgets(result, sizeof(result), fp) != NULL){
                if (send(cliS->cli_data, result, strlen(result), 0) == -1){
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
        else if(strcmp(command, "delete") == 0){
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

            if(remove(filepath) == 0){
                success = 1;
                printf("File [%s] deleted successfully.\n", filepath);
            } 
            else{
                success = 0;
                perror("File delete failed");
            }

            send(cliS->cli_data, &success, sizeof(int), 0);
        }
        else{
            printf("Client(%d): Invalid command [%s]\n", cliS->cli_data, command);
        }
        update_session(cliS->session);
    }
    
    close(cliS->cli_data);
    return NULL;
}
