#include "client_config.h"


void client_menu(int sd){
    int select;

    while(1){
        printf("1. Sign In\n");
        printf("2. Sign Up\n");
        printf("Select an option: ");
        scanf("%d", &select);
        getchar();

        if(select == 1){
            sign_in(sd);
        }
        else if(select == 2){
            sign_up(sd);
        }
        else{
            printf("Invalid option\n");
        }
    }
}

void list_files(int sd) {
    char buf[256];
    int n;

    n = recv(sd, buf, sizeof(buf) - 1, 0);
    if (n == -1) {
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';
    printf("Files:\n%s\n", buf);
}


int main(){
    int sd;
    struct sockaddr_in sin;

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket");
        exit(1);
    }

    int optvalue = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORTNUM);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(inet_pton(AF_INET, "", &sin.sin_addr) == -1){
        perror("Invalid address");
        exit(1);
    }

    if(connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1){
        perror("connect");
        exit(1);
    }

    printf("Connectede to server \n");
    client_menu(sd);
    close(sd);
    return 0;
}
