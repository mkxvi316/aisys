#ifndef COMMON_H
#define COMMON_H
#include <semaphore.h>

#define SHM_NAME "/ai_shm"
#define SEM_REQ "/sem_req"
#define SEM_RES "/sem_res"
#define MAX_BUF 4096

typedef struct {
    char question[MAX_BUF];
    char answer[MAX_BUF];
} shm_data;
#endif