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
    char id[MAXLENGTH + 1];
    char pw[MAXLENGTH + 1];
    char name[MAXLENGTH + 1];
} User;

/* 클라이언트 세션 구조체 */
typedef struct {
    int cli_data;       // 클라이언트 소켓
    int is_login;       // 로그인 여부
    Session *session;   // 세션 포인터 (로그인된 사용자의 세션 참조)
} CliSession;

/* 나야, 파일 분할 전송 */
typedef struct {
    int client_sock;
    int file_fd;
    unsigned start_offset;
    unsigned end_offset;
    char filename[MAXLENGTH];
} SendInfo;

extern pthread_mutex_t m_lock;

/* 서버 관련 함수 */
void sign_in(CliSession *session, const char *data);
void sign_up(CliSession *session);
void *client_handle(CliSession *cliS);
void *client_thread(void *arg);
void *send_handler(void *input);

#endif 