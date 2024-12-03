#ifndef SERVER_H
#define SERVER_H

#include "server_config.h"
#include "session.h"
#include <arpa/inet.h>
#include <pthread.h>
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
    int cli_data;     // 클라이언트 소켓
    int is_login;     // 로그인 여부
    Session *session; // 세션 포인터 (로그인된 사용자의 세션 참조)
} CliSession;

/* 메인에서 쓰래드 호출할때 보낼 구조체 */
typedef struct {
    int sock;     // 소켓 이름
    int user_num; // 유저 번호
} ThreadArgs;

/* 작업하는 스레드의 청크 정보 구조체 */
typedef struct {
    int cli_sock;
    int chunk_idx;
    int thread_idx;
    size_t start_offset;
    size_t data_size;
    char *file_data; // 전체 파일 데이터의 포인터
} ThreadData;

typedef struct {
    char *data;
    int data_size;
} ChunkData;

extern pthread_mutex_t m_lock;

/* 서버 관련 함수 */
void login(CliSession *session, const char *data);
void sign_up(CliSession *session);
void *home_menu(CliSession *cliS);
void main_menu(ThreadArgs arg);
void *client_thread(void *arg);
void *send_handler(void *input);

#endif