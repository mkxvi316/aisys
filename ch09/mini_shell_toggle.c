// Ctrl+\ (SIGQUIT) 사용하여 AI 모드 토글
// AI 모드에서는 exec 금지하고 안내 메시지만 출력

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define MAXLINE 1024
#define MAXARGS 64

// --- AI Mode 상태 ---
static volatile int ai_mode = 0;

// 문자열 파싱
void parse_line(char *line, char **argv) {
    int i = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && i < MAXARGS - 1) {
        argv[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[i] = NULL;
}

// --- SIGQUIT 핸들러 (Ctrl+\) ---
void handle_sigquit(int signo) {
    (void)signo;
    const char msg_on[] = "\n[AI MODE ON]\nAI-shell> ";
    const char msg_off[] = "\n[AI MODE OFF]\nmini-shell> ";

    ai_mode = !ai_mode;
    if(ai_mode) {
        write(STDOUT_FILENO, msg_on, sizeof(msg_on) - 1);
    }
    else {
        write(STDOUT_FILENO, msg_off, sizeof(msg_off) - 1);
    }
}

int main(void) {
    char line[MAXLINE];
    char *argv[MAXARGS];
    pid_t pid;
    int status;

    // 쉘의 기본 시그널 제어 ---     
    signal(SIGTTIN, SIG_IGN); // background 프로세스가 터미널에서 읽으려고 하면, 무시
    signal(SIGTTOU, SIG_IGN); // background 프로세스가 터미널에 쓰려고 하면, 무시
    signal(SIGTSTP, SIG_IGN); // Ctrl+Z (SIGTSTP) 무시 (쉘은 멈추면 안됨)
    signal(SIGQUIT, handle_sigquit); // Ctrl+\ 로 AI 모드 토글

    
    while (1) {
        // --- 프롬프트 출력 ---
        if (ai_mode)
            printf("AI-shell> ");
        else
            printf("mini-shell> ");
        fflush(stdout);

        // --- 입력 받기 ---
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break; // EOF (Ctrl+D)
        }
        if (line[0] == '\n') {
            continue;
        }

        parse_line(line, argv);
        if (argv[0] == NULL) {
            continue;
        }        if (ai_mode) {
            // exit만 허용
            if (strcmp(argv[0], "exit") == 0) break;
            
            printf("[AI] AI 기능이 아직 구현되지 않았습니다.  (exit는 가능)\n");
            continue;
        }

        // --- 내부 명령어: exit ---
        if (strcmp(argv[0], "exit") == 0) break;

        // --- 내부 명령어: cd ---
        if (strcmp(argv[0], "cd") == 0) {
            if (argv[1] == NULL) {
                fprintf(stderr, "cd: missing argument\n");
            } else if (chdir(argv[1]) != 0) {
                perror("cd");
            }
            continue;
        }

        // --- 일반 모드: fork → exec ---
        pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            setpgid(0, 0);
            execvp(argv[0], argv);
            perror("execvp");
            exit(127);
        } else {
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, pid);

            if (waitpid(pid, &status, WUNTRACED) < 0) {
                perror("waitpid");
            }

            tcsetpgrp(STDIN_FILENO, getpgrp());
        }
    }

    return 0;
}
