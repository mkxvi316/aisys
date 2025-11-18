#include "apue.h"
#include <fcntl.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <key> <input> <output>\n", argv[0]); 
        exit(1);
    }
    unsigned char key = (unsigned char)argv[1][0]; // 첫 문자를 키로 사용
    int fdin, fdout;
    char buf[4096]; // 읽기/쓰기 버퍼
    ssize_t n;

    // (1) 입력 파일 열기 → open(???, ???)
    // TODO: O_RDONLY 사용
    if ((fdin = open(argv[2], O_RDONLY)) < 0)
        err_sys("Failed to open input file");

    // (2) 출력 파일 열기 → open(???, ???, ???)
    // TODO: O_WRONLY | O_CREAT | O_TRUNC 사용, 권한은 0666
    if ((fdout = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        err_sys("Failed to open output file");

    // (3) read → for 루프에서 XOR 변환 → write
    while ((n = read(fdin, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++)
            buf[i] ^= key;   // XOR 변환
        if (write(fdout, buf, n) != n)
            err_sys("Write failed");
    }
    if (n < 0)
        err_sys("Read failed");

    // (4) 파일 디스크립터 닫기 → close(???)
    /* TODO: close input/output fd */
    close(fdin);
    close(fdout);

    printf("File transformed: %s -> %s (key=%c)\n", argv[2], argv[3], key);
    return 0;
}


// echo "Hello Security!" > plain.txt
// ./xorcrypt K plain.txt enc.txt
// ./xorcrypt K enc.txt dec.txt
// diff plain.txt dec.txt   # 차이가 없어야 성공
