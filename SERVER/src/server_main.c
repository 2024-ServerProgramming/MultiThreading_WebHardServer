#include "server.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

void handle_client(int cli){
    char buf[BUFSIZE];
    int rsize;

    CliSession cliS;
    memset(&cliS, 0, sizeof(CliSession));
    cliS.cli_data = cli;
    cliS.is_login = 0;

    while((rsize = recv(cli, buf, sizeof(buf), 0)) > 0){
        buf[rsize] = '\0';

        if(strcmp(buf, "SignUp") == 0){
            sign_up(&cliS);
        }
        else if(strcmp(buf, "SignIn") == 0){
            sign_in(&cliS);
        }
        else if(strcmp(buf, "ListFile") == 0){
            if(cliS.is_login){
                list_file(&cliS);
            }
        }
        else {
            char temp_buf[BUFSIZE];
            snprintf(temp_buf, sizeof(temp_buf), "Unknown command: %s", buf);
            strncpy(buf, temp_buf, sizeof(buf) - 1);
            send(cliS.cli_data, buf, strlen(buf), 0);
            memset(buf, 0, sizeof(buf)); 
        }
    }

    if (rsize == 0) {
        printf("Client disconnected\n");
    } 
    else if(rsize == -1){
        perror("recv");
        exit(1);
    }
    close(cliS.cli_data);
}

int main(void){
        struct sockaddr_in sin, cli;
        int sd, ns;
        socklen_t clientlen = sizeof(cli);

        if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
            perror("socket");
            exit(1);
        }
        
        int optvalue = 1;
        setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

        memset((char *)&sin, '\0', sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(PORTNUM);
        sin.sin_addr.s_addr = inet_addr("0.0.0.0");

        if(bind(sd, (struct sockaddr *)&sin, sizeof(sin))){
            perror("Bind");
            exit(1);
        }

        if(listen(sd, 10)){
            perror("Listen");
            exit(1);
        }

        srand(time(NULL));

        if(pthread_mutex_init(&m_lock, NULL) != 0){
            perror("Mutex Init Failure");
            return 1;
        }
        
        while(1){
            int *new_sock = malloc(sizeof(int));
            *new_sock = accept(sd, (struct sockaddr *)&cli, &clientlen);
            if (*new_sock < 0){
                perror("Accept");
                free(new_sock);
                continue;
            }

            pthread_t tid;
            if (pthread_create(&tid, NULL, client_thread, (void *)new_sock) != 0) {
                perror("pthread_create");
                free(new_sock);
            }
            pthread_detach(tid);
    }

    close(sd);
    return 0;
}

void *client_thread(void *arg){
    int cli_sock = *((int *)arg);
    free(arg);

    handle_client(cli_sock);

    pthread_exit(NULL);
}