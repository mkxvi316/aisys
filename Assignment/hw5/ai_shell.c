#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include "common.h"

int current_mode = 0; 
shm_data *shared_mem;
sem_t *sem_req, *sem_res;

void set_conio_mode(struct termios *old_t) {
    struct termios new_t;
    tcgetattr(STDIN_FILENO, old_t);
    new_t = *old_t;
    new_t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
}

void reset_conio_mode(struct termios *old_t) {
    tcsetattr(STDIN_FILENO, TCSANOW, old_t);
}

void execute_system_cmd(char *cmd) {
    if (strlen(cmd) == 0) return;
    char *args[64];
    int i = 0;
    char *token = strtok(cmd, " ");
    while (token) { args[i++] = token; token = strtok(NULL, " "); }
    args[i] = NULL;

    if (strcmp(args[0], "exit") == 0) exit(0);
    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024]; getcwd(cwd, sizeof(cwd)); printf("%s\n", cwd);
        return;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || chdir(args[1]) != 0) perror("cd failed");
        return;
    }

    if (fork() == 0) {
        if (execvp(args[0], args) == -1) printf("command not found\n");
        exit(1);
    } else wait(NULL);
}

int main() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shm_data));
    shared_mem = mmap(0, sizeof(shm_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    sem_req = sem_open(SEM_REQ, O_CREAT, 0666, 0);
    sem_res = sem_open(SEM_RES, O_CREAT, 0666, 0);

    struct termios old_t;
    char input[MAX_BUF];
    int idx = 0;
    memset(input, 0, MAX_BUF);

    printf("AI Assist Shell. Press Ctrl+T to switch mode.\n");
    while (1) {
        printf("\r%s> %s", current_mode ? "\x1b[35mAI\x1b[0m" : "Shell", input);
        fflush(stdout);

        set_conio_mode(&old_t);
        char c = getchar();
        reset_conio_mode(&old_t);

        if (c == 20) { // Ctrl + T
            current_mode = !current_mode;
            memset(input, 0, MAX_BUF); idx = 0;
            printf("\n[Mode Switched to %s]\n", current_mode ? "AI" : "Shell");
            continue;
        } else if (c == '\n' || c == '\r') {
            input[idx] = '\0';
            printf("\n");
            if (current_mode == 0) {
                execute_system_cmd(input);
            } else {
                memset(shared_mem->answer, 0, MAX_BUF);
                strcpy(shared_mem->question, input);
                sem_post(sem_req);
                printf("\x1b[36m🤖[AI] Waiting for response...\x1b[0m\n");

                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 600; // 10분 타임아웃 적용

                if (sem_timedwait(sem_res, &ts) == -1) {
                    printf("[AI] 10분간 응답이 없어 질의를 무시합니다.\n");
                } else {
                    printf("[AI] %s\n", shared_mem->answer);
                }
            }
            memset(input, 0, MAX_BUF); idx = 0;
        } else if (c == 127 || c == 8) { // Backspace
            if (idx > 0) { input[--idx] = '\0'; printf("\b \b"); }
        } else {
            if (idx < MAX_BUF - 1) input[idx++] = c;
        }
    }
    return 0;
}