#include "server.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>


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
        //연결확인 주석: printf("server is listening on port %d\n", PORTNUM);

        while(1){
            if((ns = accept(sd, (struct sockaddr *)&cli, &clientlen)) == -1){
                perror("Accept");
                exit(1);
            }
            
        //연결확인 주석: printf("Connected to client: %s\n", inet_ntoa(cli.sin_addr));

        handle_client(ns);

        close(ns);
    }

    close(sd);
    return 0;
}

