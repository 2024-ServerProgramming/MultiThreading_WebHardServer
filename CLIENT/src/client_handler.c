#include "client_config.h"
#include <fcntl.h>
#include <sys/time.h>

pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t available_threads_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread_pool[MAX_THREADS];
int available_threads[MAX_THREADS] = {1, 1, 1, 1, 1, 1, 1, 1};

void *send_handler(void *input) {
    ThreadData *info = (ThreadData *)input;
    int net_index = htonl(info->chunk_idx);
    int net_data_size = htonl(info->data_size);

    pthread_mutex_lock(&send_lock);
    printf("Thread %ld: Sending chunk %d (size: %ld bytes)\n", pthread_self(), info->chunk_idx, info->data_size);        // [디버깅] 전송하려는 청크 정보

    printf("Thread %ld: Sending index %d\n", pthread_self(), info->chunk_idx);           // [디버깅] 인덱스 전송
    if (send(info->cli_sock, &net_index, sizeof(net_index), 0) <= 0) {
        perror("Failed to send index");
        pthread_mutex_unlock(&send_lock);
        free(info);
        return NULL;
    }

    printf("Thread %ld: Sending data_size %ld\n", pthread_self(), info->data_size);   // [디버깅] 데이터 크기 전송
    if (send(info->cli_sock, &net_data_size, sizeof(net_data_size), 0) <= 0) {
        perror("Failed to send data size");
        pthread_mutex_unlock(&send_lock);
        free(info);
        return NULL;
    }

    // 데이터 전송
    size_t sent_bytes = 0;
    while (sent_bytes < info->data_size) {
        ssize_t sent = send(info->cli_sock, info->file_data + info->start_offset + sent_bytes, info->data_size - sent_bytes, 0);
        if (sent <= 0) {
            perror("Failed to send data");
            break;
        }
        sent_bytes += sent;
        printf("Thread %ld: Sent %zd bytes of data\n", pthread_self(), sent); 
    }
    
    printf("Thread %ld: Finished sending chunk %d\n", pthread_self(), info->chunk_idx);  // [디버깅] 청크 전송 완료
    pthread_mutex_unlock(&send_lock);


    pthread_mutex_lock(&available_threads_lock);
    available_threads[info->thread_idx] = 1;        // 스레드 다시 작업 가능으로 변경
    pthread_mutex_unlock(&available_threads_lock);

    free(info);
    return NULL;
}

void home_menu(int sd){
    while(1){
        char command[10];            // 명령어 저장
        char filename[MAX_LENGTH];
        char buf[BUF_SIZE_4095];     // 파일 송수신 버퍼
        int fd;                      // 파일 디스크립터
        unsigned fileSize;           // 파일 송수신할 때 총 파일 사이즈
        unsigned sentSize = 0;       // 파일 보낸 사이즈 합
        unsigned recvSize;           // 파일 받은 사이즈
        unsigned netFileSize;        // size_t == unsigned, 네트워크 전송용
        unsigned chunkCnt;
        unsigned netChunkCnt;
        int isnull;                  // 파일 있는지 없는지 여부 판별용 변수
        int success = 0;
        // FILE *fp = popen("clear", "r"); // 화면 지우기
        // pclose(fp);                     // 파일 포인터 닫기
        memset(command, 0, sizeof(command));

        printf("\nEnter command (download / upload / show / delete / exit): ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            fprintf(stderr, "Error: Failed to read command\n");
            break;
        }
        command[strcspn(command, "\n")] = '\0';

        if(strcmp(command, "exit") == 0){
            printf("exiting client.\n");
            close(sd); // 소켓 닫기
            break;
        }

        if(send(sd, command, strlen(command), 0) <= 0){
            perror("send cmd failed");
            break;
        }

        if (strcmp(command, "download") == 0) {
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            if (send(sd, filename, strlen(filename), 0) <= 0) {
                perror("Failed to send filename");
                continue;
            }

            if (recv(sd, &isnull, sizeof(isnull), 0) <= 0) {
                perror("Failed to receive file existence status");
                continue;
            }

            if (isnull == 0) {
                printf("File not found on server.\n");
                continue;
            }

            if (recv(sd, &netChunkCnt, sizeof(netChunkCnt), 0) <= 0) {
                perror("Failed to receive chunk count");
                continue;
            }

            chunkCnt = ntohl(netChunkCnt);
            printf("Chunk count: %u\n", chunkCnt);

            // 청크의 개수만큼 배열 생성
            ChunkData *allChunks = malloc(chunkCnt * sizeof(ChunkData));
            if (!allChunks) {
                perror("Failed to allocate memory for chunks");
                continue;
            }
            memset(allChunks, 0, chunkCnt * sizeof(ChunkData));

            // 서버에게 배열 준비 완료 메시지 전송
            char result[MAX_LENGTH]; 
            strcpy(result, "DOWNLOAD_ARRAY_READY");
            if (send(sd, result, strlen(result), 0) <= 0) {
                perror("Failed to send array ready confirmation");
                free(allChunks);
                continue;
            }

            // 청크 데이터 수신
            unsigned receivedChunks = 0;
            while (receivedChunks < chunkCnt) {
                int net_index;
                int net_data_size;
                ssize_t received;

                // 인덱스 수신
                received = recv(sd, &net_index, sizeof(net_index), MSG_WAITALL);
                if (received <= 0) {
                    perror("Failed to receive chunk index");
                    break;
                }
                int index = ntohl(net_index);
                printf("Received chunk index: %d\n", index);

                // 데이터 크기 수신
                received = recv(sd, &net_data_size, sizeof(net_data_size), MSG_WAITALL);
                if (received <= 0) {
                    perror("Failed to receive data size");
                    break;
                }
                int data_size = ntohl(net_data_size);
                printf("Received data size: %d\n", data_size);

                // 데이터 수신
                char *data = malloc(BUF_SIZE_4095);
                if (!data) {
                    perror("Failed to allocate memory for chunk data");
                    break;
                }
                size_t total_received = 0;
                while(total_received < data_size){
                    received = recv(sd, data + total_received, data_size - total_received, 0);
                    if (received <= 0) {
                        perror("Failed to receive chunk data");
                        free(data);
                        break;
                    }
                    total_received += received;
                    printf("Received %zd bytes of data\n", received);
                }

                printf("Received chunk %d (%d bytes)\n", index, data_size);  // [디버깅] 수신한 청크 정보

                if(data_size < BUF_SIZE_4095){
                    memset(data + data_size, '\n', BUF_SIZE_4095 - data_size);
                }

                allChunks[index].data = data;
                allChunks[index].data_size = data_size;
                receivedChunks++;
            }

            // 파일로 저장
            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Failed to open file for writing");
                for (unsigned i = 0; i < chunkCnt; i++) {
                    if (allChunks[i].data)
                        free(allChunks[i].data);
                }
                free(allChunks);
                continue;
            }

            for (unsigned i = 0; i < chunkCnt; i++) {       // 전체 배열에 저장된 청크 데이터 전부 기록
                if (allChunks[i].data) {
                    if(write(fd, allChunks[i].data, allChunks[i].data_size) != allChunks[i].data_size) {
                        perror("Failed to write chunk to file");
                        continue;
                    }
                    free(allChunks[i].data);
                } 
            }
            free(allChunks);
            close(fd);

            printf("File [%s] downloaded successfully.\n", filename);
        }
        else if(strcmp(command, "upload") == 0){
            memset(filename, 0, sizeof(filename));

            printf("enter filename to upload: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';

            send(sd, filename, strlen(filename), 0);

            fd = open(filename, O_RDONLY);
            if(fd < 0){
                perror("file open failed");
                isnull = 0;
                send(sd, &isnull, sizeof(isnull), 0);
                continue;
            }
            isnull = 1;
            send(sd, &isnull, sizeof(isnull), 0);

            off_t data_size = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            fileSize = data_size;

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize);

            // 청크 개수 계산 및 전송
            chunkCnt = (fileSize + BUF_SIZE_4095 - 1) / BUF_SIZE_4095;
            unsigned net_chunkCnt = htonl(chunkCnt);
            send(sd, &net_chunkCnt, sizeof(net_chunkCnt), 0);

            // 파일 전체를 메모리에 읽기
            char *file_data = malloc(fileSize);
            if (!file_data) {
                perror("Memory allocation failed");
                close(fd);
                continue;
            }
            
            ssize_t bytes_read = read(fd, file_data, fileSize);
            if(bytes_read != fileSize){
                perror("Failed to read the entire file");
                free(file_data);
                close(fd);
                continue;
            }
            close(fd);

            // 서버로부터 배열 준비 완료 확인 받기
            char result[BUF_SIZE_4095];
            int n = recv(sd, result, sizeof(result) - 1, 0);
            if (n <= 0) {
                perror("Failed to receive array ready confirmation");
                free(file_data);
                break;
            }
            result[n] = '\0';
            printf("Received from client: %s\n", result); // 디버깅 출력
            if (strcmp(result, "UPLOAD_ARRAY_READY") != 0) {
                printf("Client did not send array ready confirmation.\n");
                free(file_data);
                continue;
            }

            // 스레드풀을 이용한 청크 전송
            size_t offset = 0;
            int chunkIdx = 0;
            while (offset < fileSize) {
                ThreadData *info = malloc(sizeof(ThreadData));
                if (!info) {
                    perror("Memory allocation failed");
                    break;
                }
                info->cli_sock = sd;
                info->chunk_idx = chunkIdx;
                info->start_offset = offset;
                info->data_size = ((offset + BUF_SIZE_4095) > fileSize) ? (fileSize - offset) : BUF_SIZE_4095;
                info->file_data = file_data;

                // 스레드풀에서 사용 가능한 스레드 찾기
                int assigned = 0;
                while (!assigned) {
                    pthread_mutex_lock(&available_threads_lock);
                    for (int j = 0; j < MAX_THREADS; j++) {
                        if (available_threads[j]){
                            available_threads[j] = 0;   
                            pthread_mutex_unlock(&available_threads_lock);
                            pthread_create(&thread_pool[j], NULL, send_handler, info);
                            info->thread_idx = j;
                            assigned = 1;               
                            break;
                        }
                    }
                    if (!assigned) {
                        pthread_mutex_unlock(&available_threads_lock);
                        usleep(1000);
                    }
                }

                offset += info->data_size;
                chunkIdx++;
            }

            // 모든 스레드 종료 대기
            for (int i = 0; i < MAX_THREADS; i++) {
                pthread_join(thread_pool[i], NULL);
            }

            free(file_data);
            printf("File transfer to client completed successfully.\n");
        }

        else if(strcmp(command, "show") == 0){
            char result[BUF_SIZE_4095];
            int n = recv(sd, result, sizeof(result) - 1, 0);
            if(n <= 0){
                perror("Failed to receive");
                break;
            }

            result[n] = '\0';
            if(strcmp(result, "FILE_END") == 0){
                break;
            }

            printf("%s\n", result);
            // printf("The program will restart after 3 seconds...\n"); // 안내 문구 출력
            // sleep(3);
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
            printf("invalid command. Use 'download', 'upload', 'show', 'delete or 'exit'.\n");
        }
    }

    close(sd);
    return;
}