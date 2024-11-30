#ifndef SERVER_H
#define SERVER_H

#include "session.h"
#include "server_config.h"
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#define MAX 10

/* 사용자 정보 구조체 */
typedef struct {
    char id[MAX_LENGTH + 1];
    char pw[MAX_LENGTH + 1];
    char name[MAX_LENGTH + 1];
} User;

/* 클라이언트 세션 구조체 */
typedef struct {
    int cli_data;       // 클라이언트 소켓
    int is_login;       // 로그인 여부
    Session *session;   // 세션 포인터 (로그인된 사용자의 세션 참조)
} CliSession;

/* 파일 offset 구조체 */
typedef struct offset_info {
    int fd;            // 파일 디스크립터
    int client_sock;   // 클라이언트 소켓
    off_t start;       // 전송 시작 위치
    off_t end;         // 전송 종료 위치
    char buffer[1024]; // 데이터 버퍼
    char filename[BUFSIZE];
} OFFIN;

extern pthread_mutex_t m_lock;

/* 서버 관련 함수 */
void sign_in(CliSession *session, const char *data);
void sign_up(CliSession *session);
void *client_handle(CliSession *cliS);
void *client_thread(void *arg);

#endif 