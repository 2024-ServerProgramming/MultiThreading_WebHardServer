#include "server.h"

int tcp_listen(int host, int port, int backlog) {
    int sd = socket(AF_INET, SOCK_STREAM, 0);

    /* 포트 중복 사용 해결 */
    int optvalue = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    if (sd == -1) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in sin;

    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(host);
    sin.sin_port = htons(port);

    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin))) {
        perror("Bind");
        exit(1);
    }

    if (listen(sd, backlog) < 0) {
        perror("listen failed...");
        exit(1);
    }

    return sd;
}

void main_menu(ThreadArgs arg){
    char buf[BUFSIZE];
    int rsize;

    CliSession cliS;
    memset(&cliS, 0, sizeof(CliSession));
    cliS.cli_data = arg.sock;
    cliS.is_login = 0;

    while ((rsize = recv(arg.sock, buf, sizeof(buf), 0)) > 0) {
        buf[rsize] = '\0';

        if (strcmp(buf, "SignUp") == 0) {
            sign_up(&cliS);
        } else if (strcmp(buf, "SignIn") == 0) {
            strcpy(buf, "SignInOK");
            send(cliS.cli_data, buf, strlen(buf), 0);

            rsize = recv(arg.sock, buf, sizeof(buf) - 1, 0);
            if (rsize <= 0) {
                perror("ID and PW failed");
                break;
            }
            buf[rsize] = '\0';

            sign_in(&cliS, buf);
            if (cliS.is_login) {
                home_menu(&cliS);
                break;
            }
        } else {
            char temp_buf[BUFSIZE];
            snprintf(temp_buf, sizeof(temp_buf), "Unknown command: %s", buf);
            strncpy(buf, temp_buf, sizeof(buf) - 1);
            send(cliS.cli_data, buf, strlen(buf), 0);
            memset(buf, 0, sizeof(buf));
        }
    }

    if (rsize == 0) {
        printf("Client %d disconnected\n", arg.user_num);
    } else if (rsize == -1) {
        perror("recv");
        exit(1);
    }
    close(cliS.cli_data);
}

int main(int argc, char* argv[]){
    if(argc!=2){
        printf("%s <port> 로 입력해주세요.\n", argv[0]);
        exit(1);
    }
    int port= atoi(argv[1]);

    struct sockaddr_in cli;
    int listen_sock;
    socklen_t cli_len = sizeof(cli);

    /* 소켓 생성 및 연결 */
    listen_sock = tcp_listen(INADDR_ANY, port, 10);

    srand(time(NULL));

    if (pthread_mutex_init(&m_lock, NULL) != 0) {
        perror("Mutex Init Failure");
        return 1;
    }
    int user = 0;
    printf("서버 실행완료 클라이언트 연결요청 기다리는중 ..\n");
    while (1) { // 유저마다 소켓 만들어줌
        int *cli_sock = malloc(sizeof(int));
        *cli_sock = accept(listen_sock, (struct sockaddr *)&cli, &cli_len);

        if (*cli_sock < 0) {
            perror("소켓 연결 에러");
            free(cli_sock);
            continue;
        } else { // 연결 성공했을때 서버에 로그 띄워줌
            printf("클라이언트 %d 연결 완료\n", user);
        }
        ThreadArgs *arg = malloc(sizeof(ThreadArgs));
        arg->sock = *cli_sock;
        arg->user_num = user;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, (void *)arg) != 0) {
            perror("pthread_create");
            free(cli_sock);
        }
        pthread_detach(tid);
        user++;
    }

    close(listen_sock);
    return 0;
}

void *client_thread(void *arg) {
    ThreadArgs client_arg = *((ThreadArgs *)arg);
    free(arg);

    main_menu(client_arg);
    pthread_exit(NULL);
}