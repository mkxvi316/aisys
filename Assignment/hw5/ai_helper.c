#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "common.h"

int main() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open failed"); exit(1); }
    shm_data *shared_mem = mmap(0, sizeof(shm_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    sem_t *sem_req = sem_open(SEM_REQ, 0);
    sem_t *sem_res = sem_open(SEM_RES, 0);

    // 요청하신 프롬프트 적용
    char sys_p[1024] = "너는 우분투 전문가야. 우분투 명령어 설명을 한국어로 답변해. 그리고 간결하게 답변해.";

    printf("AI Helper (gemma3:1b) running...\n");

    while (1) {
        sem_wait(sem_req);
        printf("[Log] 질문 수신: %s\n", shared_mem->question);
        memset(shared_mem->answer, 0, MAX_BUF);

        char cmd[MAX_BUF * 2];
        snprintf(cmd, sizeof(cmd), 
                 "curl -s http://localhost:11434/api/generate -d '{"
                 "\"model\": \"gemma3:1b\","
                 "\"prompt\": \"%s 사용자의 질문: %s\","
                 "\"stream\": false"
                 "}' | python3 -c \"import sys, json; data=json.load(sys.stdin); print(data.get('response', '응답 생성 실패'))\"", 
                 sys_p, shared_mem->question);

        FILE *fp = popen(cmd, "r");
        if (fp) {
            char response[MAX_BUF] = "";
            char buffer[512];
            while (fgets(buffer, sizeof(buffer), fp)) {
                if (strlen(response) + strlen(buffer) < MAX_BUF - 50) strcat(response, buffer);
            }
            strcat(response, "\n<<<END>>>"); 
            strncpy(shared_mem->answer, response, MAX_BUF - 1);
            pclose(fp);
            printf("[Log] 응답 완료\n");
        }
        sem_post(sem_res);
    }
    return 0;
}