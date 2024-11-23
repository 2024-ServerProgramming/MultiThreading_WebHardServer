#include "server.h"
#include "session.h"
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

    // 클라이언트로부터 로그인 정보 수신
    int n = recv(cliS->cli_data, buf, sizeof(buf) - 1, 0); 
    if(n == -1){
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';

    if(sscanf(buf, "%10[^:]:%10s", s.id, s.pw) != 2){
        fprintf(stderr, "Invalid login data format: %s\n", buf);

        return;
    }

    FILE *fp = fopen("user.config", "r");
    if(fp == NULL){
        perror("user.config");
        return;
    }

    char line[BUFSIZE];
    int login_success = 0;
    while(fgets(line, sizeof(line), fp)){
        User temp;
        if(sscanf(line, "%10[^:]:%10[^:]:%10s", temp.id, temp.pw, temp.name) == 3){
            if(strcmp(temp.id, s.id) == 0 && strcmp(temp.pw, s.pw) == 0){
                login_success = 1;
                break;
            }
        }
    }
    fclose(fp);

    if (login_success){

        snprintf(buf, sizeof(buf), "Login successful. Welcome, %s!", s.id);
        cliS->is_login = 1;
        cliS->session = create_session(s.id); 
        //print_session(); 디버깅용
    } 
    else{
        strcpy(buf, "Invalid ID or password.");
    }
    send(cliS->cli_data, buf, strlen(buf), 0);
}

void create_directory(const char *username){
    char path[BUFSIZE];

    snprintf(path, sizeof(path), "./user_data/%s", username);

    mkdir("user_data", 0700);   // home 디렉터리
    if (mkdir(path, 0700) == -1 && errno != EEXIST){    // 사용자별 디렉터리 생성
        perror("Failed to create user directory");
    }
}

void sign_up(CliSession *cliS){
    User s;
    char buf[BUFSIZE];

    int n = recv(cliS->cli_data, buf, sizeof(buf) - 1, 0);
    if(n == -1){
        perror("recv");
        return;
    }
    buf[n] = '\0';

   if(sscanf(buf, "%10[^:]:%10[^:]:%10s", s.id, s.pw, s.name) != 3){
        fprintf(stderr, "Invalid user data format: %s\n", buf);
        return;
    }

    // 사용자 정보 저장
    FILE *fp = fopen("user.config", "a");
    if(fp == NULL){
        perror("Failed to open user.config");
        return;
    }
    fprintf(fp, "%s:%s:%s\n", s.id, s.pw, s.name);
    fclose(fp);

    create_directory(s.id);
    list_file(cliS);
    send(cliS->cli_data, buf, strlen(buf), 0);
}

void list_file(CliSession *cliS){
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
