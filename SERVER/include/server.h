#ifndef SERVER_H
#define SERVER_H

#include "session.h"
#include "server_config.h"

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


/* 서버 관련 함수 */
void sign_in(CliSession *session);
void sign_up(CliSession *session);
void list_file(CliSession *session);
void handle_client(int cli);
void *client_thread(void *arg);

#endif 