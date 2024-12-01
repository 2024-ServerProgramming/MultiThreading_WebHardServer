#include "client_config.h"


void client_menu(int sd){
    int select;
    while(1){
        printf("1. Sign In\n");
        printf("2. Sign Up\n");
        printf("3. Exit\n");
        printf("Select an option: ");
        scanf("%d", &select);
        getchar();

        if(select == 1){
            sign_in(sd);
        }
        else if(select == 2){
            sign_up(sd);
        }
        else if(select == 3){
            printf("Exiting client.\n");
            break;
        }
        else{
            printf("Invalid option\n");
        }
    }
}

int main(int argc, char *argv[]){

    if(argc != 2){
        printf("./%s <server ip> <port> 를 입력하시오.\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1]; 
    int port = atoi(argv[2]);
    int sd;
    struct sockaddr_in sin;

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket");
        exit(1);
    }

    int optvalue = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if(inet_pton(AF_INET, server_ip, &sin.sin_addr) == -1){
        perror("Invalid address");
        exit(1);
    }

    if(connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1){
        perror("connect");
        exit(1);
    }

    printf("Connected to server \n");
    client_menu(sd);
    close(sd);
    return 0;
}
