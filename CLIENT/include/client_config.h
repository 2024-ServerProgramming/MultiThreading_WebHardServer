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
// #define BUFSIZE 512                  // 사용자 정보 전송 버퍼 크기
#define BUF_SIZE_4095 8191              // 스레드당 파일 전송 버퍼 크기
#define MAX_LENGTH 32                   // 문자열 최대 길이
#define SESSION_TIME_OUT 86400          // 세션 타임아웃(초) - 24시간
#define DEFAULT_USER_DIR "./user_data"  // 사용자 디렉터리 경로
#define MAX_THREADS 8                   // 최대 스레드 개수
#define MAX 10

extern pthread_mutex_t m_lock;

typedef struct{
    char id[MAX + 1];
    char pw[MAX + 1];
    char name[MAX + 1];
} User;

/* 작업하는 스레드의 청크 정보 구조체 */
typedef struct {
    int cli_sock;
    int chunk_idx;
    int thread_idx;
    size_t start_offset;
    size_t data_size;
    char *file_data; 
} ThreadData;

typedef struct {
    char *data;
    int data_size;
} ChunkData;

void main_menu(int sd);
void home_menu(int sd);
void login(int sd);
void sign_up(int sd);
void get_password(char *password, size_t max_len);

#endif
