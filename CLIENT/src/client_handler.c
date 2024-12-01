#include "client_config.h"
#include <fcntl.h>
#include <sys/time.h>
pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;

void *recv_handler(void *input){
    RecvInfo *data = (RecvInfo *)input;
    char buf[BUF_SIZE];
    unsigned offset = data->start_offset;

    while(offset < data->end_offset){
        unsigned chunk_size = (data->end_offset - offset > BUF_SIZE) ? BUF_SIZE : (data->end_offset - offset);
        int recv_byte = recv(data->sock, buf, chunk_size, 0);
        if (recv_byte <= 0) break;

        if(pwrite(data->fd, buf, recv_byte, offset) <= 0){
            perror("write failed");
            break;
        }

        offset += recv_byte;
    }

    free(data);
    return NULL;
}

void client_control(int sd){
    while(1){
        char command[10]; // 명령어 저장
        char filename[MAX_LENGTH];
        char buf[BUFSIZE];
        int fd;                // 파일 디스크립터
        unsigned sentSize = 0; // 파일 받은 사이즈 합
        unsigned recvSize;     // 파일 받은 사이즈
        unsigned fileSize;     // 총 파일 사이즈
        unsigned netFileSize;  // 네트워크 전송용 파일 사이즈
        int isnull;            // 파일 존재 여부 판별용 변수
        int success = 0;

        printf("\nEnter command (get/put/show/delete/exit): ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        if(strcmp(command, "exit") == 0){
            printf("exiting client.\n");
            break;
        }

        if(send(sd, command, strlen(command), 0) <= 0){
            perror("send cmd failed");
            break;
        }

        if(strcmp(command, "get") == 0){ // 다운로드
            /*파일 이름 입력 부분*/
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            if(send(sd, filename, strlen(filename), 0) <= 0){
                perror("send filename failed");
                continue;
            }

            if(recv(sd, &isnull, sizeof(isnull), 0) <= 0){
                perror("receiving file existence fail");
                continue;
            }

            if(isnull == 0){
                printf("file not found on server.\n");
                continue;
            }

            if(recv(sd, &netFileSize, sizeof(netFileSize), 0) <= 0){
                perror("receiving file size fail");
                continue;
            }

            fileSize = ntohl(netFileSize);
            printf("downloading file [%s] (%u bytes)\n", filename, fileSize);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0){
                perror("file open failed");
                continue;
            }

            unsigned threadCnt = (fileSize + BUF_SIZE - 1) / BUF_SIZE;
            pthread_t *thread = malloc(threadCnt * sizeof(pthread_t));

            for(int i = 0; i < threadCnt; i++){
                RecvInfo *info = malloc(sizeof(RecvInfo));
                info->sock = sd;
                info->fd = fd;
                info->start_offset = i * BUF_SIZE;
                info->end_offset = (i == threadCnt - 1) ? fileSize : (i + 1) * BUF_SIZE;

                pthread_create(&thread[i], NULL, recv_handler, info);
            }

            for(int i = 0; i < threadCnt; i++){
                pthread_join(thread[i], NULL);
            }

            free(thread);

            printf("file [%s] downloaded successfully.\n", filename);
            close(fd);
        }
        else if(strcmp(command, "put") == 0){
            memset(filename, 0, sizeof(filename));

            /* 업로드할 파일 이름 입력 */
            printf("enter filename to upload: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            fd = open(filename, O_RDONLY);
            if(fd < 0){
                perror("file open failed");
                isnull = 0;
                continue;
            }

            send(sd, filename, strlen(filename), 0); // 파일 이름 전송 1

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(sd, &netFileSize, sizeof(netFileSize), 0); // 파일 크기 송신 2

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize); // 업로드할 파일 정보 출력

            sentSize = 0;
            while(sentSize < fileSize){
                recvSize = read(fd, buf, BUF_SIZE);
                if (recvSize <= 0) break;
                send(sd, buf, recvSize, 0); // 파일 순서대로 보내기 3
                sentSize += recvSize;
            }

            if(sentSize == fileSize){
                printf("file [%s] uploaded successfully.\n", filename);
            } 
            else{
                printf("file [%s] upload incomplete.\n", filename);
            }
            close(fd);
        }
        else if(strcmp(command, "show") == 0){
            char result[BUFSIZE];
            int n = recv(sd, result, sizeof(result) - 1, 0);
            if(n <= 0){
                perror("Failed to receive");
                break;
            }

            result[n] = '\0';
            if(strcmp(result, "FILE_END") == 0){
                break;
            }

            printf("%s", result);
            sleep(3);
        }
        /* 파일 삭제 */
        else if(strcmp(command, "delete") == 0){
            printf("Enter filename to delete: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

            send(sd, filename, sizeof(filename), 0); // 서버에 파일명 전송

            recv(sd, &success, sizeof(success), 0);
            if(success){
                printf("file [%s] deleted successfully.\n", filename);
            } 
            else{
                printf("file [%s] delete incomplete.\n", filename);
            }
        }
        else{
            printf("invalid command. Use 'get', 'put', 'show', 'delete or 'exit'.\n");
        }
    }

    close(sd);
    return;
}