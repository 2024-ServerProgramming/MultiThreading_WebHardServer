#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <sys/stat.h> 
#include <errno.h>     
#define PORTNUM 0
#define MAX 10

typedef struct{
    char id[MAX + 1];
    char pw[MAX + 1];
    char name[MAX + 1];
} User;

void create_user_directory(const char *username){
    char path[256];
    snprintf(path, sizeof(path), "./user_data/%s", username);
    mkdir("user_data", 0700);
    if (mkdir(path, 0700) == -1 && errno != EEXIST){
        perror("Failed to create user directory");
    }
}

void sign_up(int cli){
    User s;
    char buf[256];
    int n;

    // 클라이언트로부터 사용자 정보 수신
    n = recv(cli, buf, sizeof(buf) - 1, 0);
    if(n == -1){
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';

    sscanf(buf, "%10[^:]:%10[^:]:%10s", s.id, s.pw, s.name);

    // 사용자 정보 저장
    FILE *fp = fopen("user.config", "a");
    if(fp == NULL){
        perror("Failed to open user.config");
        return;
    }
    fprintf(fp, "%s:%s:%s\n", s.id, s.pw, s.name);
    fclose(fp);

    create_user_directory(s.id);

    // 결과 전송
    strcpy(buf, "User registered successfully");
    send(cli, buf, strlen(buf), 0);
}

void sign_in(int cli){
    User s;
    char buf[256];
    int n;

     // 클라이언트로부터 로그인 정보 수신
    n = recv(cli, buf, sizeof(buf) - 1, 0);
    if(n == -1){
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';

    sscanf(buf, "%10[^:]:%10s", s.id, s.pw);

    FILE *fp = fopen("user.config", "r");
    if(fp == NULL){
        perror("Failed to open user.config");
        return;
    }

    char line[256];
    int login_success = 0;
    while(fgets(line, sizeof(line), fp)){
        User temp;
        sscanf(line, "%10[^:]:%10[^:]:%10s", temp.id, temp.pw, temp.name);

        if (strcmp(temp.id, s.id) == 0 && strcmp(temp.pw, s.pw) == 0){
            login_success = 1;
            break;
        }
    }
    fclose(fp);

    // 결과 전송
    if(login_success){
        sprintf(buf, "Login successful. Welcome, %s!", s.id);
    } else {
        strcpy(buf, "Invalid ID or password.");
    }
    send(cli, buf, strlen(buf), 0);
}

void handle_client(int cli){
    char buf[256];
    int rsize;

    while((rsize = recv(cli, buf, sizeof(buf), 0)) == -1){
        buf[rsize] = '\0';

        if(strcmp(buf, "SignUp")){
            sign_up(cli);
        }
        else if(strcmp(buf, "SignIn")){
            sign_in(cli);
        }   
        else{
            perror("handle_client");
            exit(1);
        }
    }

    if (rsize == 0) {
        printf("Client disconnected\n");
    } else if (rsize == -1) {
        perror("recv failed");
        exit(1);
    }
}

int main(void){
        struct sockaddr_in sin, cli;
        int sd, ns, clientlen = sizeof(cli);

        memset((char *)&sin, '\0', sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(PORTNUM);
        sin.sin_addr.s_addr = inet_addr("0.0.0.0");

        if((sd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
            perror("socket");
            exit(1);
        }

        if(bind(sd, (struct sockaddr *)&sin, sizeof(sin))){
            perror("Bind");
            exit(1);
        }

        if(listen(sd, 10)){
            perror("Listen");
            exit(1);
        }

        printf("server is listening on port %d\n", PORTNUM);

        while(1){
            if((ns = accept(sd, (struct sockaddr *)&cli, &clientlen)) == -1){
                perror("Accept");
                exit(1);
            }
            
            printf("Connected to client: %s\n", inet_ntoa(cli.sin_addr));

            handle_client(ns);

        close(ns);
    }

    close(sd);
    return 0;
}