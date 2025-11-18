#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SIZE 4096

void cat(int fd) {
    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, BUF_SIZE)) > 0) {     // n은 버퍼를 받아들인 바이트 수
        if (write(STDOUT_FILENO, buf, n) != n) {    // 파일에 버퍼만큼 write
            perror("write");
            exit(1);
        }
    }
    if (n < 0) {
        perror("read");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    int fd;

    if (argc == 1) {
        // 인자가 없으면 표준 입력(stdin) 사용
        cat(STDIN_FILENO);
    } else {
        for (int i = 1; i < argc; i++) {
            fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                perror(argv[i]);
                continue;   // 에러 발생해도 다음 파일로 진행
            }
            cat(fd);
            close(fd);
        }
    }
    return 0;
}
// ./minicat file1.txt file2.txt
// ./minicat < file1.txt