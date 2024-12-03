#ifndef CONFIG_H
#define CONFIG_H

/* 공통 헤더 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 공통 설정값 */
// #define BUFSIZE  512                 // 사용자 정보 전송 버퍼 크기. 
#define BUF_SIZE_4095 4095              // 스레드당 파일 전송 버퍼 크기, BUFSIZE -> BUF_SIZE_4095로 이름 변경
#define MAX_LENGTH 32                    // 문자열 최대 길이
#define SESSION_TIME_OUT 86400          // 세션 타임아웃(초) - 24시간
#define DEFAULT_USER_DIR "./user_data"  // 사용자 디렉터리 경로
#define MAX_THREADS 8                   // 최대 스레드 개수

#endif