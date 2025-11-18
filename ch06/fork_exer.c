#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    printf("PID=%d, PPID=%d, UID=%d, EUID=%d, GID=%d, EGID=%d\n",
           getpid(), getppid(),              /* your code here */
           getuid(), geteuid(),              /* your code here */
           getgid(), getegid());            /* your code here */

    pid_t pid1, pid2, pid3;
    int status;

    // 첫 번째 자식: 정상 종료
    if ((pid1 = fork()) == 0) {             /* your code here */
        printf("[Child1] PID=%d exiting with 7\n", getpid());   /* your code here */ // 자식의 pid 가져옴
        exit(7);        /* your code here */
    }

    // 두 번째 자식: 비정상 종료
    if ((pid2 = fork()) == 0) {             /* your code here */
        printf("[Child2] PID=%d will divide by zero\n", getpid());  /* your code here */
        int x = 1 / 0;  // SIGFPE 발생 => 비정상 종료(실제 실행 안함)
        exit(1);        /* your code here */
    }

    // 부모: 자식 상태 회수
    for (int i = 0; i < 2; i++) {
        pid_t cpid = wait(&status);             /* your code here */;
        if (WIFEXITED(status)) {                /* your code here */
            printf("[Parent] Child %d exited normally, status=%d\n",
                   cpid, WEXITSTATUS(status));  /* your code here */
        } else if (WIFSIGNALED(status)) {       /* your code here */
            printf("[Parent] Child %d terminated by signal %d\n",
                   cpid, WTERMSIG(status));     /* your code here */
        }
    }
    // 세 번째 자식: 손주 생성
    if ((pid3 = fork()) == 0) {                 /* your code here */
        printf("[Child3] PID=%d will create grandchild\n", getpid());       /* your code here */
        if (fork() == 0) {                 /* your code here */
            sleep(3);
            printf("[Grandchild] PID=%d says: Hello from grandchild!\n",
                   getpid());               /* your code here */
            exit(0);                        /* your code here */
        }
        exit(0); // Child3 바로 종료 + 손주는 고아가 되어 init이 수거               /* your code here */
    }
    // 부모는 세 번째 자식만 wait (손주는 init이 수거)
    waitpid(pid3, &status, 0);                              /* your code here */
    printf("[Parent] Child3 %d finished\n", pid3);

    return 0;
}
