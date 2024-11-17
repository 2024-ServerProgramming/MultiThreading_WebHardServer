#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/stat.h>
#include<errno.h>
#include <dirent.h>
#define PORTNUM ****
#define BUFSIZE 256
#define MAX 10

typedef struct{
    char id[MAX + 1];
    char pw[MAX + 1];
    char name[MAX + 1];
}User;

typedef struct{
    int cli_data;
    int is_login;
    User user;
}CliSession;

void sign_in(CliSession *session){
    User s;
	char buf[BUFSIZE];
    int n;

    //클라이언트로부터 로그인 정보 수신
    if((recv(session->cli_data, buf, sizeof(buf) - 1, 0)) == -1){
        perror("recv");
        exit(1);
    }

    sscanf(buf, "%10[^:]:%10s", s.id, s.pw);

    FILE *fp = fopen("user.config", "r");
    if(fp == NULL){
        perror("user.config");
        return;
    }

    char line[BUFSIZE];
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

    if(login_success){
        sprintf(buf, "Login successful. Welcome, %s!", s.id);
        session->is_login = 1;
        session->user = s; 
    } 
    else{
        strcpy(buf, "Invalid ID or password.");
    }
    send(session->cli_data, buf, strlen(buf), 0);
}

void create_user_directory(const char *username){
    char path[BUFSIZE];

    snprintf(path, sizeof(path), "./user_data/%s", username);

    mkdir("user_data", 0700);
    if (mkdir(path, 0700) == -1 && errno != EEXIST){
        perror("Failed to create user directory");
    }
}

void sign_up(CliSession *session){
    User s;
    char buf[BUFSIZE];
    int n;

    if((recv(session->cli_data, buf, sizeof(buf) - 1, 0)) == -1){
        perror("recv");
        exit(1);
    }

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

    strcpy(buf, "User registered successfully");
    send(session->cli_data, buf, strlen(buf), 0);
}

void list_files(CliSession *session) {
    char path[BUFSIZE];
    char buf[1024];
    DIR *dir;
    struct dirent *ent;

    snprintf(path, sizeof(path), "./user_data/%s/", session->user.id);

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        strcpy(buf, "Failed to open user directory");
        send(session->cli_data, buf, strlen(buf), 0);
        return;
    }

    buf[0] = '\0';
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            strcat(buf, ent->d_name);
            strcat(buf, "\n");
        }
    }
    closedir(dir);

    send(session->cli_data, buf, strlen(buf), 0);
}


void handle_client(int cli){
    char buf[BUFSIZE];
    int rsize;

    CliSession session;
    session.cli_data = cli;
    session.is_login = 0;
    memset(&session.user, 0, sizeof(User));

    while((rsize = recv(cli, buf, sizeof(buf), 0)) > 0){
        buf[rsize] = '\0';

        if(strcmp(buf, "SignUp") == 0){
            sign_up(&session);
        }
        else if(strcmp(buf, "SignIn") == 0){
            sign_in(&session);
        }
        else if(strcmp(buf, "ListFile") == 0){
            if(session.is_login){
                list_files(&session);
            }
        }
        else{
            perror("handle_client");
            strcpy(buf, "Unknown command");
            send(session.cli_data, buf, strlen(buf), 0);
            exit(1);
        }

    }

    if (rsize == 0) {
        printf("Client disconnected\n");
    } 
    else if(rsize == -1){
        perror("recv");
        exit(1);
    }
    close(session.cli_data);
}

int main(void){
        struct sockaddr_in sin, cli;
        int sd, ns, clientlen = sizeof(cli);

        if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
            perror("socket");
            exit(1);
        }
        
        int optvalue = 1;
        setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

        memset((char *)&sin, '\0', sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(PORTNUM);
        sin.sin_addr.s_addr = inet_addr("****");

        if(bind(sd, (struct sockaddr *)&sin, sizeof(sin))){
            perror("Bind");
            exit(1);
        }

        if(listen(sd, 10)){
            perror("Listen");
            exit(1);
        }

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
