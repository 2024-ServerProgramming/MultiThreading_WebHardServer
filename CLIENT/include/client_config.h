#ifndef CONFIG_H
#define CONFIG_H

/* 공통 헤더 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>
#include <pthread.h>

/* 공통 설정값 */
//#define PORTNUM 7777                  // 서버 포트 번호
#define BUFSIZE 512 
#define BUF_SIZE 4095                   // 스레드당 파일 전송 버퍼 크기
#define MAX_LENGTH 512                  // 문자열 최대 길이
#define SESSION_TIME_OUT 86400          // 세션 타임아웃(초) - 24시간
#define DEFAULT_USER_DIR "./user_data"  // 사용자 디렉터리 경로
#define MAX 10

extern pthread_mutex_t m_lock;

typedef struct{
    char id[MAX + 1];
    char pw[MAX + 1];
    char name[MAX + 1];
} User;

typedef struct {
    char *data;
    int data_size;
} ChunkData;

void main_menu(int sd);
void home_menu(int sd);
void sign_in(int sd);
void sign_up(int sd);
void get_password(char *password, size_t max_len);

#endif
