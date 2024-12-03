#include "client_config.h"

void main_menu(int sd) {
    int select;
    while(1){
        // 메뉴 출력
        printf("1. Login\n"); // 로그인
        printf("2. Sign Up\n"); // 회원가입
        printf("3. Exit\n"); // 종료
        printf("Select an option: ");
        scanf("%d", &select);
        getchar();

        if (select == 1) {
            login(sd);
        } else if (select == 2) {
            sign_up(sd);
        }
        else if(select == 3){
            printf("Exiting client.\n");
            break;
        } else {
            printf("Invalid option\n");
        }
    }
}

int main(int argc, char *argv[]){

    if (argc != 3) {
        printf("%s <server ip> <port> 로 입력하시오\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1]; 
    int port = atoi(argv[2]);
    int sd;
    struct sockaddr_in sin;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }

    int optvalue = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &sin.sin_addr) == -1) {
        perror("Invalid address");
        exit(1);
    }

    if (connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        perror("connect");
        exit(1);
    }

    printf("Connected to server \n");
    main_menu(sd);
    close(sd);
    return 0;
}
