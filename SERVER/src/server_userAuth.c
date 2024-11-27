#include "server.h"
#include "session.h"
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;


void sign_in(CliSession *cliS){
    User s;
	char buf[BUFSIZE];
    memset(buf, 0, sizeof(buf));

    int n = recv(cliS->cli_data, buf, sizeof(buf) - 1, 0); 
    if(n == -1){
        perror("recv");
        return;
    }
    buf[n] = '\0';

    if(sscanf(buf, "%10[^:]:%10s", s.id, s.pw) != 2){
        fprintf(stderr, "Invalid login data format: %s\n", buf);
        return;
    }

    pthread_mutex_lock(&m_lock);
    FILE *fp = fopen("user.config", "r");
    if(fp == NULL){
        perror("user.config");
        pthread_mutex_unlock(&m_lock);
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
    pthread_mutex_unlock(&m_lock);
    
    if(login_success){
        snprintf(buf, sizeof(buf), "Login successful. Welcome, %s!", s.id);
        cliS->is_login = 1;
        cliS->session = create_session(s.id); 
        //printf("Session created for user: %s\n", cliS->session->user_id); 세션 디버깅
    } 
    else{
        strcpy(buf, "Invalid ID or password.");
    }

    send(cliS->cli_data, buf, strlen(buf), 0);
    memset(buf, 0, sizeof(buf));
}

void create_directory(const char *username){
    char path[BUFSIZE];

    snprintf(path, sizeof(path), "./user_data/%s", username);

    mkdir("user_data", 0755);                           // home 디렉터리
    if (mkdir(path, 0755) == -1 && errno != EEXIST){    // 사용자별 디렉터리 생성
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

    pthread_mutex_lock(&m_lock);

    // 사용자 정보 저장
    FILE *fp = fopen("user.config", "a");
    if(fp == NULL){
        perror("Failed to open user.config");
        pthread_mutex_unlock(&m_lock);
        return;
    }
    
    fprintf(fp, "%s:%s:%s\n", s.id, s.pw, s.name);
    fclose(fp);
    pthread_mutex_unlock(&m_lock);

    create_directory(s.id);

    snprintf(buf, sizeof(buf), "Sign up successful. Welcome, %s!", s.id);
    send(cliS->cli_data, buf, strlen(buf), 0);
}
