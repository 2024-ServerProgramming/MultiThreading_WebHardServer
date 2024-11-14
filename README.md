# MultiThreading_WebHardServer

2024 서버프로그래밍 프로젝트 입니다.

## 프로젝트 주제
- 주제: 멀티스레딩 웹 하드 서버
- 특장점: 멀티 레이어 가상 그리드
- 회의록: [서버프로그래밍 HQ Notion 링크](https://www.notion.so/131784613527809eb856db5d1c2d41d7)

## 팀 구성 및 역할

- 최원식 : 서버 통신 및 업로드/다운로드
- 김예준 : 서버 통신 및 업로드/다운로드
	- 서버에서 파일 조각을 클라이언트로 전송하는 매커니즘
	- 클라이언트의 요청을 처리하는 소켓 서버 구현
  - 멀티 스레딩 구현 (병렬 처리)
- 박찬혁 : 파일 삭제 및 조회
- 정선미 : 사용자 인증 및 접근 제어

## 프로젝트 평가
### 웹 서버 성능평가 툴을 이용
- Jmeter, Locust, K6, Apache Benchmark(ab), ...
- 여러 클라이언트 접속 부하 평가
- 제대로 된 파일 업로드 여부(정확성)

## 일정 계획
- 11/6 ~ 11/13 : 주제 및 프로젝트 목표 확정
- 11/14 ~ 11/25 : 핵심 구조 설계 및 주요 모듈 개발
- 11/25 ~ 12/1 : 주요 기능 점검
- 12/2 ~ 12/4 : 테스트 및 최종 발표 준비
- 12/5 : 최종 발표 및 프로젝트 종료
