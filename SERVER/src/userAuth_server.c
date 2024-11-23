#include "server.h"
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>


void sign_in(CliSession *cliS){
    User s;
	char buf[BUFSIZE];

    //클라이언트로부터 로그인 정보 수신
    if((recv(cliS->cli_data, buf, sizeof(buf) - 1, 0)) == -1){
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
        cliS->is_login = 1;
        cliS->session = create_session(s.id);
        char *dir_info = strstr(buf, "Current directory: ");
        if(dir_info != NULL){
            printf("%s\n", dir_info);
        }
    } 
    else{
        strcpy(buf, "Invalid ID or password.");
    }
    send(cliS->cli_data, buf, strlen(buf), 0);
}

void create_user_directory(const char *username){
    char path[BUFSIZE];

    snprintf(path, sizeof(path), "./user_data/%s", username);

    mkdir("user_data", 0700);
    if (mkdir(path, 0700) == -1 && errno != EEXIST){
        perror("Failed to create user directory");
    }
}

void sign_up(CliSession *cliS){
    User s;
    char buf[BUFSIZE];

    if((recv(cliS->cli_data, buf, sizeof(buf) - 1, 0)) == -1){
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
    send(cliS->cli_data, buf, strlen(buf), 0);
}

void list_files(CliSession *cliS) {
    char path[BUFSIZE];
    char buf[BUFSIZ + 4];
    DIR *dir;
    struct dirent *ent;

    snprintf(path, sizeof(path), "./user_data/%s/", cliS->session->user_id);

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        strcpy(buf, "Failed to open user directory");
        send(cliS->cli_data, buf, strlen(buf), 0);
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

    send(cliS->cli_data, buf, strlen(buf), 0);
}


void handle_client(int cli){
    char buf[BUFSIZE];
    int rsize;

    CliSession cliS;
    cliS.cli_data = cli;
    cliS.is_login = 0;
    memset(&cliS, 0, sizeof(CliSession));

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
                list_files(&cliS);
            }
        }
        else{
            perror("handle_client");
            strcpy(buf, "Unknown command");
            send(cliS.cli_data, buf, strlen(buf), 0);
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
    close(cliS.cli_data);
}