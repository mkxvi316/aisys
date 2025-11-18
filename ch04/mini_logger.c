#include "apue.h"
#include <fcntl.h>
#include <sys/stat.h>
#define BUFFSIZE 4096
int main(int argc, char *argv[]) {
    int fd;
    char buf[BUFFSIZE];
    ssize_t n;

    if (argc != 2)
        err_quit("usage : %s <message>", argv[0]);

    /* 로그 파일 열기: 없으면 생성, 있으면 이어쓰기 */
    if((fd = open("minilog.txt", O_WRONLY | O_APPEND | O_CREAT, 0600)) < 0)
        err_sys("open error for minilog.txt");

    /* UID + 메시지를 버퍼에 작성 */
    n = snprintf(buf, sizeof(buf), "user:%d msg:%s\n", getuid(), argv[1]);

    if(write(fd, buf, n) != n)
        err_sys("write error");

    close(fd);
    exit(0);
}