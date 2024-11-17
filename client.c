#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<termios.h>
#define PORTNUM ****
#define MAX 10

typedef struct{
    char id[MAX + 1];
    char pw[MAX + 1];
    char name[MAX + 1];
} User;


void sign_in(int sd){
    User s;
    char buf[256];
    int n;

    printf("\nId: ");
    scanf("%10s", s.id);
    getchar();

    printf("\nPassword: ");
    get_password(s.pw, sizeof(s.pw));

    strcpy(buf, "SignIn");
    send(sd, buf, strlen(buf), 0);

    sprintf(buf, "%s:%s", s.id, s.pw);
    send(sd, buf, strlen(buf), 0);

    n = recv(sd, buf, sizeof(buf) - 1, 0);
    if(n == -1){
        perror("recv");
        exit(1);
    }

    buf[n] = '\0';
    printf("%s\n", buf);

    if (strstr(buf, "Login successful") != NULL){
        strcpy(buf, "ListFile");
        send(sd, buf, strlen(buf), 0);
        
        n = recv(sd, buf, sizeof(buf) - 1, 0);
        if(n == -1){
            perror("recv");
            exit(1);
        }
        buf[n] = '\0';
        printf("Files:\n%s\n", buf);
    }
    else{
        return 0; 
    }
}

// 비밀번호 입력 시 에코 비활성화 함수
void get_password(char *password, size_t max_len){
    struct termios oldt, newt;
    int i = 0;
    int c;

    // 현재 터미널 설정을 가져옴
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;~
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

void sign_up(int sd){
    User s;
    char buf[256];
    int n;

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

    n = recv(sd, buf, sizeof(buf), 0);
    if(n == -1){
        perror("recv");
        exit(1);
    }

    buf[n] = '\0';
    printf("%s\n", buf);
}

void client_menu(int sd){
    int select;

    while(1){
        printf("1. Sign In\n");
        printf("2. Sign Up\n");
        printf("Select an option: ");
        scanf("%d", &select);
        getchar();

        if(select == 1){
            sign_in(sd);
        }
        else if(select == 2){
            sign_up(sd);
        }
        else{
            printf("Invalid option\n");
        }
    }
}

void list_files(int sd) {
    char buf[256];
    int n;

    n = recv(sd, buf, sizeof(buf) - 1, 0);
    if (n == -1) {
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';
    printf("Files:\n%s\n", buf);
}


int main(){
    int sd;
    struct sockaddr_in sin;

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket");
        exit(1);
    }

    int optvalue = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORTNUM);
    sin.sin_addr.s_addr = inet_addr("****");

    if(inet_pton(AF_INET, "", &sin.sin_addr) == -1){
        perror("Invalid address");
        exit(1);
    }

    if(connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1){
        perror("connect");
        exit(1);
    }

    printf("Connectede to server \n");
    client_menu(sd);
    close(sd);
    return 0;
}
