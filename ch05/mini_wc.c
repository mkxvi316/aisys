#include <stdio.h>
#include <stdlib.h>

#define BUFFSIZE 4096

int main(int argc, char *argv[]) {    
    FILE *fp = NULL;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 2;
    }
    fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    // 라인 단위 읽기 (fgets)
    int line_count = 0;
    char linebuf[BUFFSIZE];
    while (fgets(linebuf, sizeof(linebuf), fp)) {
        line_count++;
    }
    
    rewind(fp); // 파일 처음으로 이동    

    // 파일 길이 구하기 (fseek, ftell))
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    
    rewind(fp);

    // 문자/단어 단위 읽기 (fgetc)
    int word_count = 0;
    int c;
    int in_word = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (c == ' ' || c == '\n' || c == '\t') {
            in_word = 0;
        } else {
            if (!in_word) {
                word_count++;
                in_word = 1;
            }
        }
    }

    printf("Line count : %d\n", line_count);
    printf("Word count : %d\n", word_count);
    printf("File size : %ld \n", filesize);

    fclose(fp);
    return 0;
}
