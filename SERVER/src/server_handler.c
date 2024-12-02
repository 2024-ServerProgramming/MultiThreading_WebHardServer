#include "server.h"
#include <fcntl.h>
#include <netinet/in.h>

pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t available_threads_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread_pool[MAX_THREADS];
int available_threads[MAX_THREADS] = {1, 1, 1, 1, 1, 1, 1, 1};

void *send_handler(void *input) {
    ThreadData *info = (ThreadData *)input;
    int net_index = htonl(info->chunk_idx);
    int net_data_size = htonl(info->data_size);

    pthread_mutex_lock(&send_lock);
    //printf("Thread %ld: Sending chunk %d (size: %ld bytes)\n", pthread_self(), info->chunk_idx, info->data_size);        // [디버깅] 전송하려는 청크 정보

    //printf("Thread %ld: Sending index %d\n", pthread_self(), info->chunk_idx);           // [디버깅] 인덱스 전송
    if (send(info->cli_sock, &net_index, sizeof(net_index), 0) <= 0) {
        perror("Failed to send index");
        pthread_mutex_unlock(&send_lock);
        free(info);
        return NULL;
    }

    //printf("Thread %ld: Sending data_size %ld\n", pthread_self(), info->data_size);   // [디버깅] 데이터 크기 전송
    if (send(info->cli_sock, &net_data_size, sizeof(net_data_size), 0) <= 0) {
        perror("Failed to send data size");
        pthread_mutex_unlock(&send_lock);
        free(info);
        return NULL;
    }

    //데이터 전송
    size_t sent_bytes = 0;
    while (sent_bytes < info->data_size) {
        ssize_t sent = send(info->cli_sock, info->file_data + info->start_offset + sent_bytes, info->data_size - sent_bytes, 0);
        if (sent <= 0) {
            perror("Failed to send data");
            break;
        }
        sent_bytes += sent;
        //printf("Thread %ld: Sent %zd bytes of data\n", pthread_self(), sent); 
    }
    
    //printf("Thread %ld: Finished sending chunk %d\n", pthread_self(), info->chunk_idx);  // [디버깅] 청크 전송 완료
    pthread_mutex_unlock(&send_lock);


    pthread_mutex_lock(&available_threads_lock);
    available_threads[info->thread_idx] = 1;        // 스레드 다시 작업 가능으로 변경
    pthread_mutex_unlock(&available_threads_lock);

    free(info);
    return NULL;
}
   
void *home_menu(CliSession *cliS){
    char command[10]; // 명령어 저장
    char filename[MAXLENGTH];
    char buf[BUF_SIZE_4095];    // 파일 송수신 버퍼, BUFSIZE -> BUF_SIZE_4095로 수정
    int fd;                // 파일 디스크립터
    unsigned fileSize;     // 파일 송수신할 때 총 파일 사이즈
    unsigned sentSize = 0; // 파일 보낸 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
    unsigned recvSize;     // 파일 받은 사이즈
    unsigned netFileSize;  // size_t == unsigned, 네트워크 전송용
    unsigned chunkCnt;
    int isnull;            // 파일 있는지 없는지 여부 판별용 변수
    int success = 0;

    find_session(cliS->session->session_id);

    while(1){
        memset(command, 0, sizeof(command));

        if(recv(cliS->cli_data, command, sizeof(command), 0) <= 0){
            printf("client disconnected.\n");
            break;
        }

        if(strcmp(command, "get") == 0){
            memset(filename, 0, sizeof(filename));

            if(recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0){
                printf("receiving filename failed\n");
                break;
            }

            //printf("Client(%d): Requesting file [%s]\n", cliS->cli_data, filename);

            char filepath[BUF_SIZE_4095]; // BUFSIZE -> BUF_SIZE_4095로 수정
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

            off_t data_size = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);

            fileSize = data_size;

            // 청크 개수 계산 및 전송
            chunkCnt = (fileSize + BUF_SIZE_4095 - 1) / BUF_SIZE_4095;
            unsigned net_chunkCnt = htonl(chunkCnt);
            send(cliS->cli_data, &net_chunkCnt, sizeof(net_chunkCnt), 0);

            //printf("Total chunks to send: %u\n", chunkCnt); // 디버깅 출력

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

            // 클라이언트로부터 배열 준비 완료 확인 받기
            char result[BUF_SIZE_4095]; // BUFSIZE -> BUF_SIZE_4095로 수정
            int n = recv(cliS->cli_data, result, sizeof(result) - 1, 0);
            if (n <= 0) {
                perror("Failed to receive array ready confirmation");
                free(file_data);
                break;
            }
            result[n] = '\0';
            printf("Received from client: %s\n", result); // 디버깅 출력
            if (strcmp(result, "ARRAY_READY") != 0) {
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
                info->cli_sock = cliS->cli_data;
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
                            available_threads[j] = 0;   // 작업해야하면 0으로 변경
                            pthread_mutex_unlock(&available_threads_lock);
                            pthread_create(&thread_pool[j], NULL, send_handler, info);
                            info->thread_idx = j;
                            assigned = 1;               // 작업이 끝났으면 다시 1로 변경
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
        /* put 명령어 */
        else if(strcmp(command, "put") == 0){
            memset(filename, 0, sizeof(filename));

            if(recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0){
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Uploading file [%s]\n", cliS->cli_data, filename);

            char filepath[BUF_SIZE_4095]; // BUFSIZE -> BUF_SIZE_4095로 수정
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
                recvSize = recv(cliS->cli_data, buf, BUF_SIZE_4095, 0);
                if (recvSize <= 0)
                    break;
                ssize_t bytes_written = pwrite(fd, buf, recvSize, sentSize); 
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
            char result[BUF_SIZE_4095]; // BUF_SIZE -> BUF_SIZE_4095로 수정

            char filepath[BUF_SIZE_4095]; // BUFSIZE -> BUF_SIZE_4095로 수정
            snprintf(filepath, sizeof(filepath), "ls -a ./user_data/%s", cliS->session->user_id);

            fp = popen(filepath, "r");
            if (fp == NULL) {
                perror("Failed to run command");
                pclose(fp);
                continue;
            }

            // 명령어 실행 결과를 읽어와 클라이언트로 전송
            while (fgets(result, sizeof(result), fp) != NULL){
                if (send(cliS->cli_data, result, strlen(result), 0) == -1){
                    perror("Failed to send");
                    pclose(fp);
                    break;
                }
            }

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

            char filepath[BUF_SIZE_4095]; // BUF_SIZE -> BUF_SIZE_4095로 수정
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
        update_session(cliS->session);
    }
    close(cliS->cli_data);
    return NULL;
}