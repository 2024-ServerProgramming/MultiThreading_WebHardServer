#include "client_config.h"
#include <fcntl.h>
#include <sys/time.h>
pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;

void client_control(int sd){
    while(1){
        char command[10]; // 명령어 저장
        char filename[MAX_LENGTH];
        char buf[BUFSIZE];
        int fd;                // 파일 디스크립터
        unsigned fileSize;     // 파일 송수신할 때 총 파일 사이즈
        unsigned sentSize = 0; // 파일 보낸 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
        unsigned recvSize;     // 파일 받은 사이즈
        unsigned netFileSize;  // size_t == unsigned, 네트워크 전송용
        unsigned chunkCnt;
        unsigned netChunkCnt;
        int isnull;            // 파일 있는지 없는지 여부 판별용 변수
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

        if (strcmp(command, "get") == 0) {
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
            char result[BUFSIZE];
            strcpy(result, "ARRAY_READY");
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
                char *data = malloc(BUF_SIZE);
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

                // 데이터 크기가 BUF_SIZE보다 작을 경우 나머지를 '\n'으로 채우기
                if(data_size < BUF_SIZE){
                    memset(data + data_size, '\n', BUF_SIZE - data_size);
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