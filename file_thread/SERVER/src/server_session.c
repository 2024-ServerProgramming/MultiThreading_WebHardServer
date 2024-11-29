#include "session.h"

static Session *session_list = NULL;

void generate_session_id(char *session_id){
		const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
		for(int i = 0; i < MAX_LENGTH; i++){
			session_id[i] = chars[rand() % strlen(chars)];
		}
		session_id[MAX_LENGTH] = '\0';
}

Session *create_session(const char *user_id){
		Session *new_session = malloc(sizeof(Session));
		if(new_session == NULL){
			perror("malloc");
			return NULL;
		}

		generate_session_id(new_session->session_id);
		strncpy(new_session->user_id, user_id, MAX_LENGTH);
		new_session->last_active = time(NULL);
		new_session->next = session_list;
		session_list = new_session;
		return new_session;
}

Session *find_session(const char *session_id){
	Session *cur = session_list;
	while(cur !=  NULL){
		if(strcmp(cur->session_id, session_id) == 0){	
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}

void update_session(Session *session) {
    session->last_active = time(NULL);
}

void remove_expired_sessions(){
	Session *cur = session_list;
	Session *prev = NULL;
	time_t now = time(NULL);
	
	while(cur != NULL){
		if(difftime(now, cur->last_active) > SESSION_TIME_OUT){
				if(prev == NULL){
					session_list = cur->next;
				}
				else{
					prev->next = cur->next;
				}
				Session *temp = cur;
				cur = cur->next;
				free(temp);
		}
		else{
			prev = cur;
			cur = cur->next;
		}
	}
}

/* 세션 디버깅용 함수 
void print_session(){
    Session *cur = session_list;
    printf("Active sessions:\n");
    while (cur != NULL){
        printf("User ID: %s, Session ID: %s, Last Active: %ld\n", cur->user_id, cur->session_id, cur->last_active);
        cur = cur->next;
    }
}
*/