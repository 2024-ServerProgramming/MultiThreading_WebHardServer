#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAXLINE 511
#define BUFSIZE 256

void errquit(char *mesg) { //
    perror(mesg);          // 에러메세지 cli 로 출력
    exit(0);
}

int tcp_listen(int host, int port, int backlog) {
    int sd;                      // 소켓 디스크립터
    struct sockaddr_in servaddr; // IPv4 주소 정보를 관리하는 구조체
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) <
        0) {                    // AF_INET => Pv4 주소 체계, SOCK_STREAM => TCP(연결 지향형) 소켓을 의미
        errquit('socket fail'); // 소켓 생성하면 socket() 는 -1 반환, 성공하면 양수 반환
    }
    bzero((char *)&servaddr, sizeof(servaddr)); // servaddr 초기화
    servaddr.sin_family = AF_INET;              // IPv4 주소체계 사용
    servaddr.sin_addr.s_addr = htonl(host); // 네트워크로 데이터를 전송할때 빅엔디언으로 정규화 해서 보내야함.
    servaddr.sin_port = htons(port); // 포트번호도 빅엔디언으로 정규화
    if (bind(sd, (struct sockadd *)&servaddr, sizeof(servaddr)) < 0) {
        // 서버에서 바인딩해서 서버 소켓에 아이피, 포트번호 등록해줌.
        errquit("bind fail");
    }

    listen(sd, backlog); // 클라이언트 요청 대기큐 만들기.
    return sd;           // 소켓 디스크립터 리턴 (소켓을 다루는 고유한 번호)
}

int main(int argc, char *argv[]) {
    struct sockaddr_in cliaddr;
    int listen_sock, accp_sock;
    char command[5], filename[MAXLINE], dir[MAXLINE], buf[BUFSIZE];
    int addrlen, success = 0, nbyte;
    FILE *file;
    size_t fsize;

    if (argc != 2) {
        printf("사용법 : %s[port]\n", argv[0]);
        exit(0);
    }

    listen_sock =
        tcp_listen(INADDR_ANY, atoi(argv[1]),
                   5); // 여기서 INADDR_ANY unsigned long 이라 int host 에 안맞을거 같은데.. 8바이트 -> 4바이트??
    // 파라미터 각각 INADDR_ANY = 이컴퓨터가 쓰는 IP아무거나 되는거 통해서 클라이언트 요청 받는다. int host에 대응
    // atoi(argv[1]) => int port 에 대응. 입력 파라미터로 port 번호받음 ex) ./a.out 8080
    // 5 => 대기큐 사이즈 정하기 listen(sd,5);
    if (listen_sock == -1) {
        errquit("tcp_listen function fail");
    } // listen_sock 은 서버 소켓이다.

    addrlen = sizeof(cliaddr); // cliaddr 은 sockaddr_in 형이잖아 16바이트 씀 결국 16바이트 길이 나옴.
    accp_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, addrlen);
    // accept(소켓 디스크립터, struct sockaddr* 형 연결된 클라이언트 주소와 포트 담기는 구조체, sockaddr 구조체 크기)
    while (1) {                         // accp_sock 은 연결된 클라이언트의 소켓정보를 가짐
        recv(accp_sock, command, 5, 0); // 명령어 수신
        // 클라이언트와 연결된 소켓을 accp_sock 즉 첫번째 파라미터로
        // 요청 받은 데이터를 저장할 버퍼 char command[5]
        // 버퍼 사이즈 5로 설정
        // 마지막 파라미터는 읽을 데이터 유형 옵션임 0 => 일반데이터 받을거라는 뜻

        if (!strcmp(command, "cd\n")) { // cd 명령어 처리 (경로 변경)?? 근데 이게 왜 필요한거지?? -> 프로세스에서 뭐
                                        // 저장하거나 할때 원하는 경로에 저장하기 위해.
            recv(accp_sock, dir, MAXLINE, 0); // 경로수신
            if (chdir(dir) == 0)
                success = 1; // 경로변경 성공
            else
                success = 0;
            send(accp_sock, &success, sizeof(int), 0); // 성공여부 전송
            // 클라이언트 소켓 디스크립터, 성공여부 데이터 주소, 크기, 일반데이터 옵션
        } else if (!strcmp(command, "get\n")) {    // get 명령어 처리(다운로드)
            recv(accp_sock, filename, MAXLINE, 0); // 파일 이름 수신 MAXLINE의 크기는 511 바이트
            file = fopen(filename, "rb");          // file name => 경로포함 파일 이름,
            // rb모드 => r= 읽기전용, b= 바이너리모드(데이터 그대로) ==> 바이너리 데이터 그대로 읽기.(텍스트던 뭐던)
            int isnull = 0;
            if (file == NULL) {                              // 파일을 봤는데 아무것도 없다면,
                isnull = 1;                                  // 널 플래그 1로해주고
                send(accp_sock, &isnull, sizeof(isnull), 0); // 클라이언트에게 널 사인보냄
                // accp_sock : 클라이언트 소켓 디스크립터, &isnull : 널플래그 주소, sizeof(isnull) : 사이즈,  0 : 일반
                // 전송 옵션
                continue;
            }
            send(accp_sock, &isnull, sizeof(isnull),
                 0); // 파일 존재 여부 전송 (널 아니니까 이건 널 아님 이라고 전송 isnull =0)

            fseek(file, 0, SEEK_END); // 파일 포인터를 끝으로 이동
            fsize = ftell(file);      // 파일 포인터 시작기준 오프셋 읽음 = 파일의크기
            // fseek(file, 0, SEEK_END); fsize = ftell(file); 은 파일 크기를 구하는 국룰 코드다.
            fseek(file, 0, SEEK_SET); // 파일 포인터 다시 처음으로 이동

            size_t size = htonl(fsize); // 사이즈 빅엔디언으로 정규화 (네트워크로 클라이언트에 전달할거)
            // success, isnull 둘다 send 할때 빅엔디언 정규화 (htonl(fsize)) 안했는데 왜 얘만하냐?
            // success, isnull 은 둘다 단일바이트 즉, 0 또는 1밖에 저장안되므로 빅엔디언이든 뭐든 상관 x
            // 하지만, 사이즈는 여러바이트로 구성되어있으니 빅엔디언(네트워크 바이트 순서)으로 정규화 해서 클라이언트에
            // 전송해야한다. 다중바이트 데이터는 빅엔디언으로 변환 (네트워크 전송 표준) 을 해서 보내야함.
            send(accp_sock, &size, sizeof(fsize), 0); // 파일 크기 전송
            int nsize = 0;
            while (nsize != fsize) {
                int fpsize = fread(buf, 1, BUFSIZE, file);
                nsize += fpsize;
                send(accp_sock, buf, fpsize, 0);
            }
            fclose(file);
        } else if (!strcmp(command, "put\n")) { // put 명령어 처리(업로드)
            int isnull = 0;
            recv(accp_sock, &isnull, sizeof(isnull), 0);
            if (isnull == 1) {
                continue;
            }

            recv(accp_sock, filename, MAXLINE, 0);     // 파일 이름 수신
            file = fopen(filename, "wb");              // 쓰기전용, 이진파일모드로 열기
            recv(accp_sock, &fsize, sizeof(fsize), 0); // 파일 크기 수신??

            nbyte = BUFSIZE;
            while (nbyte >= BUFSIZE) { // nbyte == BUFSIZE 라는건, 데이터 읽을게 딱 떨어졌거나, 더있다는 뜻
                nbyte = recv(accp_sock, buf, BUFSIZE, 0); // buf에 저장 다 해놓고, nbyte에 읽은 바이트수 리턴
                success = fwrite(buf, sizeof(char), nbyte, file); // file 에다가 버퍼내용 nbyte 만큼 기록
                if (nbyte < BUFSIZE)
                    success = 1; // 버퍼 사이즈 n 바이트 보다 작으면 끝났다는 뜻 success =1;
            }
            send(accp_sock, &success, sizeof(int), 0); // 성공여부 전송
            fclose(file);
        } else if (!strcmp(command, "quit")) {        // 클라이언트 한테 입력받은 command 가 quit 일때
            int isexit = 0;                           // 초기화
            recv(accp_sock, &isexit, sizeof(int), 0); // 진짜 나갈건지 클라이언트 답변을 받아옴

            if (isexit) { // 결국 진짜 나간대
                printf("프로그램을 종료합니다.\n");
                close(listen_sock); // 서버 소켓 닫기
                close(accp_sock);   // 클라이언트 소켓닫기
                exit(0);            // 프로그램종료
            }
        }
    }
}
