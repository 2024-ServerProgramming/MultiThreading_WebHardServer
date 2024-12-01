#ifndef SESSION_H
#define SESSION_H

#include "server_config.h"


/* 세션 정보 구조체 */
typedef struct Session{
    char session_id[MAXLENGTH + 1];    // 세션 id 저장
    char user_id[MAXLENGTH + 1];       // 로그인한 유저id
    time_t last_active;                 // 마지막 활동 시간
    struct Session *next;
}Session;

/* 세션 관련 함수 */
void generate_session_id(char *session_id);
Session *create_session(const char *user_id);
Session *find_session(const char *session_id);
void update_session(Session *session);
void remove_expired_sessions();
void print_session(void);

#endif 
