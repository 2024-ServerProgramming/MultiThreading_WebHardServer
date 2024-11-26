#include "server.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int tcp_listen(int host, int port, int backlog){
    int sd = socket(AF_INET, SOCK_STREAM, 0);

     /* 포트 중복 사용 해결 */
    int optvalue = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    if(sd == -1){
            perror("socket");
            exit(1);
    }
    struct sockaddr_in sin;

    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(host);
    sin.sin_port = htons(port);

    if(bind(sd, (struct sockaddr *)&sin, sizeof(sin))){
            perror("Bind");
            exit(1);
    }

    if(listen(sd, backlog) < 0){
        perror("listen failed...");
        exit(1);
    }

    return sd;
}

void start_index(int cli_sock){
    char buf[BUFSIZE];
    int rsize;

    CliSession cliS;
    memset(&cliS, 0, sizeof(CliSession));
    cliS.cli_data = cli_sock;
    cliS.is_login = 0;

    while((rsize = recv(cli_sock, buf, sizeof(buf), 0)) > 0){
        buf[rsize] = '\0';

        if(strcmp(buf, "SignUp") == 0){
            sign_up(&cliS);
        }
        else if(strcmp(buf, "SignIn") == 0){
            sign_in(&cliS);
            if (cliS.is_login) {
                client_handle(&cliS);
                break;
            }
        }
        else{
            char temp_buf[BUFSIZE];
            snprintf(temp_buf, sizeof(temp_buf), "Unknown command: %s", buf);
            strncpy(buf, temp_buf, sizeof(buf) - 1);
            send(cliS.cli_data, buf, strlen(buf), 0);
            memset(buf, 0, sizeof(buf)); 
        }
    }

    if(rsize == 0){
        printf("Client disconnected\n");
    } 
    else if(rsize == -1){
        perror("recv");
        exit(1);
    }
    close(cliS.cli_data);
}

int main(void){
    struct sockaddr_in cli;
    int listen_sock;
    socklen_t cli_len = sizeof(cli);

    /* 소켓 생성 및 연결 */
    listen_sock = tcp_listen(INADDR_ANY, 8080, 10);

    srand(time(NULL)); // 접속 시간을 위한 난수 생성

    if(pthread_mutex_init(&m_lock, NULL) != 0){ 
        perror("Mutex Init Failure");
        return 1;
    }
    
    while(1){
        int *cli_sock = malloc(sizeof(int));
        *cli_sock = accept(listen_sock, (struct sockaddr *)&cli, &cli_len);
        if(*cli_sock < 0){
            perror("Accept");
            free(cli_sock);
            continue;
        }

        pthread_t tid;
        if(pthread_create(&tid, NULL, client_thread, (void *)cli_sock) != 0){
            perror("pthread_create");
            free(cli_sock);
        }
        pthread_detach(tid);
    }

    close(listen_sock);
    return 0;
}

void *client_thread(void *arg){
    int cli_sock = *((int *)arg);
    free(arg);

    start_index(cli_sock);
    pthread_exit(NULL);
}