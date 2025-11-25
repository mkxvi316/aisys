#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

// --- 상수 및 전역 변수 설정 ---

#define NUM_THREADS 2
#define T1_INDEX 0
#define T2_INDEX 1

// 데드락 감지 시간 관련 상수
#define CHECK_INTERVAL_MS 100 // 0.1초마다 검사
#define DEADLOCK_WAIT_TIME 5 // 총 5초 대기
#define DEADLOCK_THRESHOLD (DEADLOCK_WAIT_TIME * 1000 / CHECK_INTERVAL_MS) // 5초 = 50회 검사

// 스레드 정보 구조체
typedef struct {
    int index;
    char *name;
    char *pattern;
} ThreadInfo;

// 자원(뮤텍스) 정보 구조체
typedef struct {
    pthread_mutex_t lock;
    int owner_index; // 현재 락을 보유한 스레드의 인덱스 (T1_INDEX, T2_INDEX 등, -1: 미보유)
    char *name; // 자원 이름 (A 또는 B)
} Resource;

// --- Wait-for Graph (WFG) 관련 전역 변수 ---
// wait_for_graph[i][j] = 1 : 스레드 i가 스레드 j가 보유한 자원을 기다림
int wait_for_graph[NUM_THREADS][NUM_THREADS] = {0}; 
pthread_mutex_t graph_mutex; // WFG 접근 보호를 위한 뮤텍스
pthread_mutex_t print_mutex; // 출력 보호를 위한 뮤텍스 (로그 메시지 보호)

// 자원 A와 B
Resource R[NUM_THREADS]; 
ThreadInfo Threads[NUM_THREADS];

// --- DFS를 위한 상태 정의 ---
#define WHITE 0 
#define GRAY 1  
#define BLACK 2 

// --- 함수 선언 ---
void initialize_system();
void cleanup_system();
int dfs_cycle(int u, int color[]);
int check_deadlock();
void *monitor_thread_func(void *arg);
void *thread_func_t1(void *arg);
void *thread_func_t2(void *arg);
int custom_lock(Resource *res, int waiter_index);
void custom_unlock(Resource *res, int releaser_index);

// --- 1, 2번 항목: WFG 및 DFS 기반 사이클 감지 구현 ---

/**
 * @brief DFS를 사용하여 그래프에서 사이클을 탐지합니다. 
 */
int dfs_cycle(int u, int color[]) {
    color[u] = GRAY; 

    for (int v = 0; v < NUM_THREADS; v++) {
        if (wait_for_graph[u][v] == 1) { // u가 v를 기다림
            if (color[v] == GRAY) {
                return 1; // 사이클 발견 (Gray -> Gray)
            }
            if (color[v] == WHITE) {
                if (dfs_cycle(v, color)) {
                    return 1; 
                }
            }
        }
    }

    color[u] = BLACK; 
    return 0;
}

/**
 * @brief 전체 WFG에 대해 데드락(사이클)이 있는지 확인합니다.
 */
int check_deadlock() {
    int color[NUM_THREADS];
    memset(color, WHITE, sizeof(color));

    pthread_mutex_lock(&graph_mutex);
    for (int i = 0; i < NUM_THREADS; i++) {
        if (color[i] == WHITE) {
            if (dfs_cycle(i, color)) {
                pthread_mutex_unlock(&graph_mutex);
                return 1; // 데드락 감지
            }
        }
    }
    pthread_mutex_unlock(&graph_mutex);
    return 0; // 데드락 없음
}


/**
 * @brief 락 요청을 처리하고 WFG에 간선을 추가합니다.
 */
int custom_lock(Resource *res, int waiter_index) {
    pthread_mutex_lock(&print_mutex);
    printf(" [스레드 %s] 자원 %s 잠금 시도\n", 
           Threads[waiter_index].pattern, res->name);
    pthread_mutex_unlock(&print_mutex);

    // 락 시도
    if (pthread_mutex_trylock(&res->lock) == 0) {
        // 잠금 성공
        res->owner_index = waiter_index;

        pthread_mutex_lock(&print_mutex);
        printf(" [스레드 %s] 자원 %s 잠금 성공\n", 
               Threads[waiter_index].pattern, res->name);
        pthread_mutex_unlock(&print_mutex);
        
        return 0;
    } else {
        // 잠금 실패 (자원이 점유 중)
        int owner_index = res->owner_index;

        // WFG에 간선 추가: waiter_index가 owner_index를 기다림
        pthread_mutex_lock(&graph_mutex);
        wait_for_graph[waiter_index][owner_index] = 1;
        pthread_mutex_unlock(&graph_mutex);

        // 4번 항목: 출력 형식 준수
        pthread_mutex_lock(&print_mutex);
        printf(" [감시기] 스레드 %s -> 스레드 %s를 기다림 (자원 %s)\n",
               Threads[waiter_index].pattern, Threads[owner_index].pattern, res->name);
        pthread_mutex_unlock(&print_mutex);

        // 실제 잠금을 기다림 (여기서 영원히 대기하게 됨)
        pthread_mutex_lock(&res->lock);
        
        // 잠금 성공 후 (데드락 해소 후 락 획득): WFG에서 간선 제거
        pthread_mutex_lock(&graph_mutex);
        wait_for_graph[waiter_index][owner_index] = 0;
        pthread_mutex_unlock(&graph_mutex);

        // 소유자 업데이트
        res->owner_index = waiter_index;
        
        return -1; 
    }
}

/**
 * @brief 락 해제를 처리하고 소유자를 초기화합니다.
 */
void custom_unlock(Resource *res, int releaser_index) {
    // 락 해제 시에도 WFG 간선 제거 로직이 필요할 수 있지만,
    // 이 과제는 데드락 감지 후 종료가 목표이므로 단순화
    res->owner_index = -1; 
    pthread_mutex_unlock(&res->lock);
}


// --- 수정된 2-2번 항목: 감시기 스레드 구현 (5초 동안 유지 확인) ---

/**
 * @brief 데드락 감시 스레드 함수 (5초 동안 데드락 유지 확인 후 종료)
 */
void *monitor_thread_func(void *arg) {
    int deadlock_counter = 0;

    while (1) {
        usleep(CHECK_INTERVAL_MS * 1000); // 0.1초 대기

        if (check_deadlock()) {
            deadlock_counter++;
            
            if (deadlock_counter >= DEADLOCK_THRESHOLD) {
                // 5초 동안(50회) 데드락이 해소되지 않고 유지됨
                
                // 4번 항목: 출력 형식 준수
                pthread_mutex_lock(&print_mutex);
                printf("\n === 데드락 감지됨! (%d초 경과) ===\n", DEADLOCK_WAIT_TIME);
                printf(" 감지된 스레드들 :\n");
                printf(" - 스레드 %s\n", Threads[T1_INDEX].pattern);
                printf(" - 스레드 %s\n", Threads[T2_INDEX].pattern);
                printf("\n 프로그램을 종료합니다.\n");
                pthread_mutex_unlock(&print_mutex);
                
                cleanup_system();
                exit(0);
            }
        } else {
            // 데드락이 해소되었으므로 카운터 초기화
            deadlock_counter = 0;
        }
    }
    return NULL;
}

// --- 3번 항목: 데드락 재현 ---

/**
 * @brief 스레드 T1의 작업 함수 (A->B 순서)
 */
void *thread_func_t1(void *arg) {
    // 1. 자원 A 잠금 시도 (R[0])
    custom_lock(&R[0], T1_INDEX);
    
    // T2가 B 락을 성공할 시간을 부여하여 교착 상태를 유도
    usleep(500000); 

    // 2. 자원 B 잠금 시도 (R[1]) - T2가 B를 가지고 있어 대기
    custom_lock(&R[1], T1_INDEX);

    // *이 코드는 실행되지 않음 (데드락 감지 후 종료)*
    custom_unlock(&R[1], T1_INDEX);
    custom_unlock(&R[0], T1_INDEX);
    return NULL;
}

/**
 * @brief 스레드 T2의 작업 함수 (B->A 순서)
 */
void *thread_func_t2(void *arg) {
    // 1. 자원 B 잠금 시도 (R[1])
    custom_lock(&R[1], T2_INDEX);
    
    // T1이 A 락을 성공할 시간을 부여하여 교착 상태를 유도
    usleep(500000); 

    // 2. 자원 A 잠금 시도 (R[0]) - T1이 A를 가지고 있어 대기
    custom_lock(&R[0], T2_INDEX);

    // *이 코드는 실행되지 않음 (데드락 감지 후 종료)*
    custom_unlock(&R[0], T2_INDEX);
    custom_unlock(&R[1], T2_INDEX);
    return NULL;
}


// --- 초기화 및 실행 ---

void initialize_system() {
    // 1. 뮤텍스 및 자원 초기화
    pthread_mutex_init(&graph_mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);

    pthread_mutex_init(&R[0].lock, NULL);
    R[0].owner_index = -1;
    R[0].name = strdup("A");

    pthread_mutex_init(&R[1].lock, NULL);
    R[1].owner_index = -1;
    R[1].name = strdup("B");

    // 2. 스레드 정보 초기화
    Threads[T1_INDEX].index = T1_INDEX;
    Threads[T1_INDEX].name = strdup("T1");
    // LaTeX 출력을 위해 \rightarrow를 사용함
    Threads[T1_INDEX].pattern = strdup("T1(A->B)");

    Threads[T2_INDEX].index = T2_INDEX;
    Threads[T2_INDEX].name = strdup("T2");
    Threads[T2_INDEX].pattern = strdup("T2(B->A)");
}

void cleanup_system() {
    // 메모리 해제
    free(R[0].name);
    free(R[1].name);
    free(Threads[T1_INDEX].name);
    free(Threads[T1_INDEX].pattern);
    free(Threads[T2_INDEX].name);
    free(Threads[T2_INDEX].pattern);
    
    // 뮤텍스 파괴
    pthread_mutex_destroy(&R[0].lock);
    pthread_mutex_destroy(&R[1].lock);
    pthread_mutex_destroy(&graph_mutex);
    pthread_mutex_destroy(&print_mutex);
}

int main() {
    pthread_t tids[NUM_THREADS];
    pthread_t monitor_tid;

    initialize_system();

    // 1. 감시 스레드 시작
    if (pthread_create(&monitor_tid, NULL, monitor_thread_func, NULL) != 0) {
        perror("Monitor thread creation failed");
        cleanup_system();
        return 1;
    }

    // 2. T1, T2 스레드 시작
    if (pthread_create(&tids[T1_INDEX], NULL, thread_func_t1, NULL) != 0) {
        perror("T1 thread creation failed");
        cleanup_system();
        return 1;
    }

    if (pthread_create(&tids[T2_INDEX], NULL, thread_func_t2, NULL) != 0) {
        perror("T2 thread creation failed");
        cleanup_system();
        return 1;
    }

    // 메인 스레드는 스레드가 종료되기를 기다림 (데드락 감지 후 monitor_thread가 exit(0)로 종료시킬 것임)
    pthread_join(tids[T1_INDEX], NULL);
    pthread_join(tids[T2_INDEX], NULL);
    pthread_join(monitor_tid, NULL);

    cleanup_system();
    return 0;
}