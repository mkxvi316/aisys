#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXLINE 1024
#define MAXARGS 64

// 문자열을 공백 기준으로 나누어 argv 배열 생성
void parse_line(char *line, char **argv) {
    int i = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && i < MAXARGS - 1) {
        argv[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[i] = NULL;
}

int main(void) {
    char line[MAXLINE];
    char *argv[MAXARGS];
    pid_t pid;
    int status;

    while (1) {
        printf("mini-shell> ");
        fflush(stdout);

        // 입력 없으면 EOF 처리
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        // 공백/엔터만 입력 시 무시
        if (line[0] == '\n') continue;

        // 명령 파싱
        parse_line(line, argv);
        if (argv[0] == NULL) continue;

        // 내장 명령어 처리
        if (strcmp(argv[0], "exit") == 0) {
            break;
        }
        if (strcmp(argv[0], "cd") == 0) {
            if (argv[1] == NULL) {
                fprintf(stderr, "cd: missing argument\n");
            } else {
                if (chdir(argv[1]) != 0) {
                    perror("cd");
                }
            }
            continue;
        }

        // fork → exec → wait
        pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            // 자식 프로세스: 명령 실행
            execvp(argv[0], argv);
            perror("execvp"); // 실패 시
            exit(127);
        } else {
            // 부모 프로세스: 자식 종료 대기
            if ((waitpid(pid, &status, 0)) < 0) {
                perror("waitpid");
            }
        }
    }
    return 0;
}
