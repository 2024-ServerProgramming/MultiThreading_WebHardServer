#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#define MAX 10

typedef struct{
    char id[MAX + 1];
    char pw[MAX + 1];
    char name[MAX + 1];
} User;

void create_user_directory(const char *username){
    char path[256];
    snprintf(path, sizeof(path), "./user_data/%s", username);
    mkdir("user_data", 0700);
    if (mkdir(path, 0700) == -1 && errno != EEXIST){
        perror("Failed to create user directory");
    }
}

// 비밀번호 입력 시 에코 비활성화
void get_password(char *password, size_t max_len){
    struct termios oldt, newt;
    int i = 0;
    int c;

    // 현재 터미널 설정을 가져옴
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while((c = getchar()) != '\n' && c != EOF && i < max_len - 1){
        password[i++] = c;
    }
    password[i] = '\0';

    // 이전 터미널 설정 복원
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n"); 
}

void SignUp(){
    User s;
    printf("\nId: ");
    scanf("%10s", s.id);
    while(getchar() != '\n'); 

    printf("\nPassword: ");
    get_password(s.pw, sizeof(s.pw));

    printf("\nName: ");
    scanf("%10s", s.name);
    while(getchar() != '\n'); 

    FILE *fp = fopen("user.config", "a");
    if(fp == NULL){
        perror("Failed to open user.config");
        return;
    }
    fprintf(fp, "%s:%s:%s\n", s.id, s.pw, s.name);  // 사용자 정보 입력
    fclose(fp);

    create_user_directory(s.id);    // 사용자 개인 디렉터리 생성 함수
    printf("User registered successfully\n");
}

int SignIn(){
    User s;
    char input_id[MAX + 1], input_pw[MAX + 1];

    for(int i = 0; i < 3; i++){
        system("clear");
        printf("\nId: ");
        scanf("%10s", input_id);
        while(getchar() != '\n'); 

        printf("\nPassword: ");
        get_password(input_pw, sizeof(input_pw));

        FILE *fp = fopen("user.config", "r");

        if(fp == NULL){
            perror("Failed to open user.config");
            return 0;
        }

        char line[256];
        int login_success = 0;
        while(fgets(line, sizeof(line), fp)){
            sscanf(line, "%10[^:]:%10[^:]:%10s", s.id, s.pw, s.name);

            if (strcmp(s.id, input_id) == 0 && strcmp(s.pw, input_pw) == 0){   // 사용자 정보가 존재하는지 확인
                system("clear");
                printf("Login successful. Welcome, %s!\n", s.name);
                login_success = 1;
                break;
            }
        }
        fclose(fp);

        if(login_success){
            return 1; // 로그인 성공
        } 
        else{
            printf("Invalid ID or password. Attempts remaining: %d\n", 2 - i);
            printf("Press Enter to try again...\n");
            while(getchar() != '\n');
        }
    }
    return 0; // 로그인 실패
}

int main(void){
    int choice;
    while (1){
        system("clear");
        printf("1. Sign In\n");
        printf("2. Sign Up\n");
        printf("Select an option: ");
        scanf("%d", &choice);
        while(getchar() != '\n'); 

        if (choice == 1){
            if (SignIn()){
                system("clear");
                printf("다음 페이지\n");
                return 0;
            }
            else{
                printf("SignIn failed.\n");
            }
        } 
        else if(choice == 2){
            SignUp();
        } 
        else{
            printf("Invalid option\n");
        }
    }
    return 0;
}
