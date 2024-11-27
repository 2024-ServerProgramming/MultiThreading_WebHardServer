#include <pthread.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h> // 파일 디스크립터 헤더

typedef struct offset_info {
    int fd;
    int fd_m;
    off_t start; // 시작부분
    off_t end;   // 마지막 붑분
} OFFIN;

void *process_range(void *off) {
    OFFIN *off_info = (OFFIN *)off;
    int fd = off_info->fd;
    off_t start = off_info->start;
    off_t end = off_info->end;
    int fd_m = off_info->fd_m; // 이건 필요없음

    lseek(fd, start, SEEK_SET); // 파일 포인터를 시작 위치로 이동

    off_t remaining = end - start;
    char buffer[1024];

    while (remaining > 0) {
        // 반복문 한번당 읽어야하는 바이트
        ssize_t byte_to_read = (remaining < 1024) ? remaining : 1024;
        // 실제로 읽어서 버퍼에 저장한 바이트
        ssize_t bytes_read = read(fd, buffer, byte_to_read);
        // fd_m 에 버퍼에 있는거 저장함
        ssize_t bytes_written = write(fd_m, buffer, bytes_read);

        remaining -= bytes_read; // 읽은만큼 총 남은 비트에서 까줌
    }
}

int main() {
    OFFIN off[4];
    pthread_t pthreads[4];
    int fd = open("example.txt", O_RDONLY);                                 // 읽기모드로 열음
    int fd_m = open("merged_file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644); // 쓰기모드로 열음

    off_t file_size = lseek(fd, 0, SEEK_END); // 파일 사이즈 구하기

    for (int i = 0; i < 4; i++) { // 파일을 읽어들여서 4등분으로 나누기
        off[i].start = 0 + (file_size / 4) * i;
        off[i].end = (file_size / 4) * (i + 1);
        off[i].fd = fd;
        off[i].fd_m = fd_m;
    }
    for (int i = 0; i < 4; i++) {
        pthread_create(&pthreads[i], NULL, process_range, &off[i]);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(pthreads[i], NULL);
    }
}