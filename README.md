# MultiThreading_WebHardServer

2024 서버프로그래밍 프로젝트 입니다.

## 프로젝트 주제
- 주제: 멀티스레딩 웹 하드 서버
- 특장점: 멀티 레이어 가상 그리드
  	- 서버와 클라이언트에서 파일 조각을 서로 전송하고 온전하게 받는 매커니즘
	- 청크 사이즈로 나눈 파일조각들을 멀티 스레딩을 통해 전송 => 전송 속도 향상

## 팀 구성 및 역할
### 최원식
- 서버 통신 및 업로드 / 다운로드
- 성능 평가를 위한 테스트 프로그램 구현 (다중 사용자 접속시 안정성)

### 김예준
- 서버 통신 및 업로드 / 다운로드
- 프로젝트 계획 발표 (단일 사용자 내에서 용량별 성능 비교)

### 박찬혁
- 파일 삭제 및 조회 기능 구현
- 프로젝트 최종 발표

### 정선미
- 사용자 인증 및 접근 제어
- 파일 삭제 및 조회 기능 구현
- 서버 통신 및 업로드 / 다운로드

## 프로젝트 평가
### 테스트 클라이언트 생성 방식을 통한 성능 평가
- 여러 클라이언트 접속 부하 평가
- 제대로 된 파일 업로드 여부(정확성)

## 일정 계획
- 11/6 ~ 11/13 : 주제 및 프로젝트 목표 확정
- 11/14 ~ 11/25 : 핵심 구조 설계 및 주요 모듈 개발
- 11/25 ~ 12/1 : 주요 기능 점검
- 12/2 ~ 12/4 : 테스트 및 디버깅, 최종 발표 준비
- 12/4 : 최종 발표 및 프로젝트 종료, 동료 평가
---------
> ## 실행 방법
1. 서버에 SERVER 디렉터리, 클라이언트에 CLIENT 디렉터리를 위치시킨다.
2. 서버는 "./SERVER" 디렉터리 위치로 이동 후 "make" 명령으로 빌드한다.
3. 서버는 "./bin/server.exe <포트번호>" 명령으로 서버 코드를 실행한다.
4. 클라이언트는 "./CLIENT" 디렉터리 위치로 이동 후 "make" 명령으로 빌드한다.
5. 클라이언트는 "./bin/client.exe <서버IP주소> <포트번호>" 명령으로 클라이언트 코드를 실행한다.
### 실행 이후 과정
6. 클라이언트 화면에서 로그인(1) 또는 회원가입(2)을 선택한다.
7. 사용자 인증에 성공하면 home menu로 진입하며, 명령어를 입력해 원하는 기능을 실행한다.
   - 7-1. download: 서버의 자신의 디렉터리에 있는 파일 중 다운로드할 파일명을 입력한다.
   	- 7-1-(1). 다운로드된 파일은 CLIENT 디렉터리에 생성된다.
   - 7-2. upload: CLIENT 디렉터리에 있는 파일을 서버로 업로드한다.
   	- 7-2-(1). 업로드된 파일은 사용자의 디렉터리 내에 생성된다.
   - 7-3. show: 자신의 서버 디렉터리 파일 목록을 클라이언트 화면에 출력한다.
   - 7-4. delete: 서버 디렉터리에 있는 파일 중 삭제할 파일명을 입력한다.
   	- 7-4-(1). 서버는 해당 사용자의 디렉터리 내에서 파일을 삭제한다.
   - 7-5. exit: 클라이언트 화면이 home menu에서 main menu로 이동한다.

* 사용자 개인 디렉터리: ./SERVER/user_data/<사용자id>
