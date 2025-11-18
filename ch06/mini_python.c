#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

jmp_buf env;

void process_line(char *line) {
    line[strcspn(line, "\n")] = 0;      // 개행 제거

    if (strncmp(line, "print ", 6) == 0) {
        printf("%s\n", line + 6);       // your code here (문자열 출력)
    }
    else if (strncmp(line, "set ", 4) == 0) {
        char *kv = line + 4;
        if (strchr(kv, '=') == NULL) {
            fprintf(stderr, "SyntaxError: set KEY=VALUE 필요\n");   // your code here (에러 메시지 + longjmp)
            longjmp(env, 1);            
        }

        /* malloc으로 전체 문자열 복사 */
        char *buf = malloc(strlen(kv) + 1);             // your code here (malloc으로 문자열 공간 확보)
        if (buf == NULL) {
            fprintf(stderr, "malloc failed\n");     
            longjmp(env, 1);
        }
        strncpy(buf, kv, strlen(kv));                   // your code here (strncpy로 전체 복사) 
        buf[strlen(kv)] = '\0';

        if (putenv(buf) != 0) {                         // your code here (putenv 호출)
            fprintf(stderr, "putenv failed\n");
            free(buf); // 실패 시 안전하게 해제
            longjmp(env, 1);
        }

        printf("환경변수 등록: %s\n", buf);                 // your code here (성공 메시지 출력)
        /* buf는 환경에서 참조 중 → free 금지 */
    }
    else if (strcmp(line, "exit") == 0) {
        printf("종료합니다.\n");                            // your code here (종료)
        exit(0);
    }
    else {
        // 환경변수 조회 시도
        char *val = getenv(line);                        // your code here (getenv로 조회, 없으면 longjmp)
        if (val) {
            printf("%s\n", val);
        } else {
            fprintf(stderr, "NameError: '%s' 정의되지 않음\n", line);
            longjmp(env, 1);
        }
    }
}

int main(void) {
    char line[128];

    while (1) {
        if (setjmp(env) != 0)
            printf("에러 발생 → REPL 복구 완료\n");

        printf(">>> ");
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        process_line(line);
    }
    return 0;
}
