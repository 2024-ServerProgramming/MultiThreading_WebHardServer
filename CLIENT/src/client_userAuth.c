#include "client_config.h"

// 비밀번호 입력 시 에코 비활성화 함수
void get_password(char *password, size_t max_len) {
    struct termios oldt, newt;
    int i = 0;
    int c;

    // 현재 터미널 설정을 가져옴
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while ((c = getchar()) != '\n' && c != EOF && i < max_len - 1) {
        password[i++] = c;
    }
    password[i] = '\0';

    // 이전 터미널 설정 복원
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
}

void login(int sd) {
    User s;
    char buf[256];
    int n;

    strcpy(buf, "Login");
    send(sd, buf, strlen(buf), 0);

    n = recv(sd, buf, sizeof(buf) - 1, 0);
    if (n == -1) {
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';

    if (strcmp(buf, "LoginOK") != 0) {
        printf("Unexpected response: %s\n", buf);
        return;
    }

    printf("\nId: ");
    scanf("%10s", s.id);
    getchar();

    printf("\nPassword: ");
    get_password(s.pw, sizeof(s.pw));

    sprintf(buf, "%s:%s", s.id, s.pw);
    send(sd, buf, strlen(buf), 0);

    n = recv(sd, buf, sizeof(buf) - 1, 0);
    if (n == -1) {
        perror("recv");
        exit(1);
    }

    buf[n] = '\0';
    printf("%s\n", buf);

    if (strstr(buf, "Login successful") != NULL) {
        // FILE *fp = popen("clear", "r"); // 화면 지우기
        // pclose(fp);                     // 파일 포인터 닫기
        home_menu(sd);
    } else {
        return;
    }
}

void sign_up(int sd) {
    User s;
    char buf[256];

    printf("\nId: ");
    scanf("%10s", s.id);
    getchar();

    printf("\nPassword: ");
    get_password(s.pw, sizeof(s.pw));

    printf("\nName: ");
    scanf("%10s", s.name);
    getchar();

    strcpy(buf, "SignUp");
    send(sd, buf, strlen(buf), 0);

    sprintf(buf, "%s:%s:%s", s.id, s.pw, s.name);
    send(sd, buf, strlen(buf), 0);

    int n = recv(sd, buf, sizeof(buf) - 1, 0);
    if (n == -1) {
        perror("recv");
        exit(1);
    }

    buf[n] = '\0';
    printf("%s\n", buf);

    if (strstr(buf, "Sign up successful") != NULL) {
        // FILE *fp = popen("clear", "r"); // 화면 지우기
        // pclose(fp);                     // 파일 포인터 닫기 
        home_menu(sd);
    } else {
        return;
    }
}