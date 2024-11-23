#ifndef CONFIG_H
#define CONFIG_H

/* 공통 헤더 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 공통 설정값 */
#define PORTNUM 7777           // 서버 포트 번호
#define BUFSIZE 256            // 버퍼 크기
#define MAX_LENGTH 32          // 문자열 최대 길이
#define SESSION_TIME_OUT 86400  // 세션타임아웃(초) - 24시간
#define DEFAULT_USER_DIR "./user_data" // 사용자 디렉터리 경로

#endif