#include "client_config.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>


void client_control(int sd){
    while(1){
        char command[10]; // 명령어 저장
        char filename[MAX_LENGTH];
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
        if(strcmp(command, "exit") == 0){
            printf("exiting client.\n");
            break;
        }

        // 서버로 명령어 전송
        if(send(sd, command, strlen(command), 0) == -1){
            perror("send cmd failed...");
            break;
        }

        if(strcmp(command, "get") == 0){ // 다운로드
            /*파일 이름 입력 부분*/        
            printf("Enter filename to download: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0'; // 아... 이런게 있네
            /* 여기까지 */

            send(sd, filename, strlen(filename), 0); 

            if(recv(sd, &isnull, sizeof(isnull), 0) == -1){ 
                perror("receiving file existence fail");
                continue;
            }

            if(isnull == 0){
                printf("file not found on server.\n");
                continue;
            }

            if(recv(sd, &netFileSize, sizeof(netFileSize), 0) == -1){ // 전달 받은 파일 사이즈 3
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

            sentSize = 0;
            while(sentSize < fileSize){
                recvSize = recv(sd, buf, BUFSIZE, 0); // 파일 전송 받기 4
                if (recvSize <= 0) break;
                write(fd, buf, recvSize);
                sentSize += recvSize;
            }

            if(sentSize == fileSize){
                printf("file [%s] downloaded successfully.\n", filename);
            } 
            else{
                printf("file [%s] download incomplete.\n", filename);
            }
            close(fd);

        }
        else if (strcmp(command, "put") == 0){
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

            send(STDERR_FILENO, filename, strlen(filename), 0); // 파일 이름 전송 1

            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(sd, &netFileSize, sizeof(netFileSize), 0); // 파일 크기 송신 2

            printf("Uploading file [%s] (%u bytes)\n", filename, fileSize); // 업로드할 파일 정보 출력

            int sentSize = 0;
            while(sentSize < fileSize){
                recvSize = read(fd, buf, BUFSIZE);
                if (recvSize <= 0) break;
                send(sd, buf, recvSize, 0); // 파일 순서대로 보내기 3
                sentSize += recvSize;
            }

            if(sentSize == fileSize){
                printf("file [%s] uploaded successfully.\n", filename);
            } else {
                printf("file [%s] upload incomplete.\n", filename);
            }
            close(fd);
        } 
        else{
            printf("invalid command. Use 'get', 'put', or 'exit'.\n");
        }
    }

    close(sd);
    return 0;

}