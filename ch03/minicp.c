#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    int src, dst;
    ssize_t nread;
    char buf[BUF_SIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        exit(1);
    }

    src = open(argv[1], O_RDONLY);
    if (src < 0) {
        perror("open source");
        exit(1);
    }

    dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        perror("open destination");
        close(src);
        exit(1);
    }

    while ((nread = read(src, buf, BUF_SIZE)) > 0) {
        if (write(dst, buf, nread) != nread) {
            perror("write");
            close(src);
            close(dst);
            exit(1);
        }
    }
    if (nread < 0) perror("read");

    close(src);
    close(dst);
    return 0;
}
