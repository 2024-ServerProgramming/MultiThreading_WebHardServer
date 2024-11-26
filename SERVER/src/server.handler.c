#include "server.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>


void *client_handle(CliSession *cliS){
    char command[5];  // 명령어 저장
    char filename[MAX_LENGTH];
    char buf[BUFSIZE];
    int fd;           // 파일 디스크립터 
    unsigned fileSize;     // 파일 송수신할 때 총 파일 사이즈
    unsigned sentSize;     // 파일 보낸 사이즈 합, 계속 recvSize에서 더해줘서 fileSize까지 받도록
    unsigned recvSize;     // 파일 받은 사이즈
    unsigned netFileSize; // size_t == unsigned, 네트워크 전송용
    int isnull;       // 파일 있는지 없는지 여부 판별용 변수

    while(1){
        memset(command, 0, sizeof(command)); // command 버퍼 비우기

        if(recv(cliS->cli_data, command, sizeof(command), 0) <= 0){ // recv 이상한 값 오는 경우
            printf("client disconnected.\n");
            break;
        }

        /* get 명령어 */
        if(strcmp(command, "get") == 0){ // 사용자 입장에서 다운로드
            memset(filename, 0, sizeof(filename)); // filename 버퍼 비우기

            if(recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0){ //1. 사용자에게 파일 이름 받는 부분
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Requesting file [%s]\n", cliS->cli_data, filename); // 디버깅용...

            /* 사용자 디렉토리에서 파일 열기 */
            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            fd = open(filename, O_RDONLY);
            if (fd < 0) {
                perror("file open failed");
                isnull = 0;
                send(cliS->cli_data, &isnull, sizeof(isnull), 0); // 파일 열기 실패했다고 전송 2
                continue;
            }

            isnull = 1;
            send(cliS->cli_data, &isnull, sizeof(isnull), 0); // 파일 열기 성공했다고 전송 2

            /* 자, 여기서부터 잘 확인하기 */
            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            netFileSize = htonl(fileSize);
            send(cliS->cli_data, &netFileSize, sizeof(netFileSize), 0); // 2. 파일 사이즈 전송 3
            /* 여기까지 잘 작동하는가? */

            sentSize = 0; // 여기서 파일 전송 시작하는데 sentSize가 fileSize일 때 까지 전송함
            while(sentSize < fileSize){
                recvSize = read(fd, buf, BUFSIZE);
                if (recvSize <= 0) break;
                send(cliS->cli_data, buf, recvSize, 0); // 파일 전송 4
                sentSize += recvSize;
            }

            printf("File [%s] sent to client (%u bytes)\n", filename, fileSize);
            close(fd);

        } 
        /* put 명령어 */
        else if(strcmp(command, "put") == 0){      
            memset(filename, 0, sizeof(filename));

            if(recv(cliS->cli_data, filename, sizeof(filename), 0) <= 0){ // 파일 이름 받기 1
                printf("receiving filename failed\n");
                break;
            }

            printf("Client(%d): Uploading file [%s]\n", cliS->cli_data, filename);

            /* 사용자 디렉토리에 파일 생성 */
            char filepath[BUFSIZE];
            snprintf(filepath, sizeof(filepath), "./user_data/%s/%s", cliS->session->user_id, filename);

            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0){
                perror("file creation failed");
                break;
            }

            if(recv(cliS->cli_data, &netFileSize, sizeof(netFileSize), 0) <= 0){ // 파일 크기 수신 2
                printf("receiving file size failed\n");
                close(fd);
                break;
            }

            // 네트워크 바이트 순서를 호스트 바이트 순서로 변환
            fileSize = ntohl(netFileSize);
            printf("Receiving file [%s] (%u bytes)\n", filename, fileSize); // 파일 정보 출력

            sentSize = 0;
            while(sentSize < fileSize){
                recvSize = recv(cliS->cli_data, buf, BUFSIZE, 0); // 파일 순서대로 받기 3
                if (recvSize <= 0) break;
                write(fd, buf, recvSize);
                sentSize += recvSize;
            }

            if(sentSize == fileSize){
                printf("File [%s] received successfully.\n", filename);
            }
            else{
                printf("File [%s] transfer incomplete.\n", filename);
            }
            close(fd);
        } 
        else{
            printf("Client(%d): Invalid command [%s]\n", cliS->cli_data, command);
        }
    }

    close(cliS->cli_data);
    return;
}