#ifndef CONFIG_H
#define CONFIG_H

/* 공통 헤더 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 공통 설정값 */
// #define BUFSIZE  512                 // 사용자 정보 전송 버퍼 크기. BUF_SIZE_4095랑 헷갈림.
// 둘이 왜 구분했는지는 알겠는데 그냥 BUF_SIZE를 하나로 통일하는 게 나을 거 같아서 주석 처리. 
// 실제로 이거 때문에 오류 발생 (예상)
#define BUF_SIZE_4095 4095              // 스레드당 파일 전송 버퍼 크기, BUFSIZE -> BUF_SIZE_4095로 이름 변경
#define MAXLENGTH 32                    // 문자열 최대 길이
#define MAX_LINE 512                    //
#define SESSION_TIME_OUT 86400          // 세션 타임아웃(초) - 24시간
#define DEFAULT_USER_DIR "./user_data"  // 사용자 디렉터리 경로
#define MAX_THREADS 8                   // 최대 스레드 개수

#endif