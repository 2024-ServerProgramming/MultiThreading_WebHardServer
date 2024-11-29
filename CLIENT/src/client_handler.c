#include "client_config.h"
#include <fcntl.h>
#include <sys/time.h>
pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct offset_info {
    int fd;
    int client_sock;
    off_t start; // 시작부분
    off_t end;   // 마지막 부분
    char buffer[1024];
} OFFIN;

void What_I_received(OFFIN off) {
    printf("클라이언트 받은 크기 : %d\n", sizeof(off));
    printf("클라이언트 받은 fd : %d\n", off.fd);
    printf("클라이언트 받은 start : %d\n", off.start);
    printf("클라이언트 받은 end : %d\n", off.end);
    printf("클라이언트 받은 client_sock : %d\n", off.client_sock);
    printf("클라이언트 받은 buffer : %s\n", off.buffer);
}

void *process_range(void *off) {
    OFFIN *off_info = (OFFIN *)off;
    int fd = off_info->fd;
    off_t start = off_info->start;
    off_t end = off_info->end;
    int client_sock = off_info->client_sock;

    lseek(fd, start, SEEK_SET); // 파일 포인터를 시작 위치로 이동

    off_t remaining = end - start;

    while (remaining > 0) {
        ssize_t recv_bytes = recv(client_sock, off_info->buffer, sizeof(off_info->buffer), 0);
        if (recv_bytes <= 0) {
            perror("Receive failed");
            break;
        }

        // 파일 오프셋 이동
        if (lseek(fd, off_info->start, SEEK_SET) == (off_t)-1) {
            perror("Failed to seek");
            close(fd);
            pthread_exit(NULL);
        }

        ssize_t bytes_written = write(fd, off_info->buffer, recv_bytes);
        if (bytes_written < 0) {
            perror("Failed to write");
            close(fd);
            pthread_exit(NULL);
        }

        remaining -= recv_bytes;
    }
}

void show_file_list(int sd){
    char *command = "ls -al";
    system(command);
    return  0;
}

void client_control(int sd){
    struct timeval start, end;

    while(1){
        char command[10];       // 명령어 저장
        char filename[MAX_LENGTH];
        char buf[BUFSIZE];
        char quit;              // 파일 조회용 변수
        int fd;                 // 파일 디스크립터 
        unsigned sentSize;      // 파일 받은 사이즈 합
        unsigned recvSize;      // 파일 받은 사이즈
        unsigned fileSize;      // 총 파일 사이즈
        unsigned netFileSize;   // 네트워크 전송용 파일 사이즈
        int isnull;             // 파일 존재 여부 판별용 변수
        int success = 0;

        sleep(1);
        (void)system("clear");

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

            while(sentSize < fileSize){
                pthread_mutex_lock(&m_lock);
                ssize_t recv_bytes = recv(sd, buf, sizeof(buf), 0);
                pthread_mutex_unlock(&m_lock);
                if (recv_bytes <= 0) {
                    perror("Receive failed");
                    break;
                }
                if (write(fd, buf, recv_bytes) <= 0) {
                    perror("Write failed");
                    break;
                }
                sentSize += recv_bytes;
            }
            gettimeofday(&end, NULL);
            double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
            printf("Time taken: %f seconds\n", time_taken);

            if(sentSize == fileSize){
                printf("file [%s] downloaded successfully.\n", filename);
            } 
            else{
                printf("file [%s] download incomplete.\n", filename);
            }
            
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

            if(send(sd, filename, strlen(filename), 0) <= 0){ 
                perror("send filename failed...");
                close(fd);
                continue;
            }

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(sd, &netFileSize, sizeof(netFileSize), 0); \

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize); // 업로드할 파일 정보 출력

            sentSize = 0;
            while(sentSize < fileSize){
                recvSize = read(fd, buf, BUFSIZE);
                if(recvSize <= 0) break;
                send(sd, buf, recvSize, 0); 
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
            if (strcmp(result, "FILE_END") == 0) {
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

            send(sd, filename, sizeof(filename), 0);  // 서버에 파일명 전송

            recv(sd, &success, sizeof(success), 0);  
            if (success){
                printf("file [%s] downloaded successfully.\n", filename);
            }
            else{
                printf("file [%s] download incomplete.\n");
            }
        }
        else{
            printf("invalid command. Use 'get', 'put', 'show', 'delete or 'exit'.\n");
        }
    }

    close(sd);
    return 0;

}