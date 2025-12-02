#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

// 간단한 미니 셸: ai_helper_chat 프로그램과 파이프로 통신

int main(void) {
    // 파이프를 위한 파일 디스크립터 배열
    int pipe_parent_to_child[2];  // your code here
    int pipe_child_to_parent[2];  // your code here

    // 파이프 생성: 부모 -> 자식, 자식 -> 부모
    // 둘 중 하나라도(hint : or 연산) 실패하면 종료
    if (pipe(pipe_parent_to_child) != 0 || pipe(pipe_child_to_parent) != 0) {       // your code here
        perror("pipe");
        return 1;
    }

    // 자식 프로세스 생성
    pid_t pid = fork();  // your code here

    if (pid == 0) {
        // 자식 프로세스: ai_helper_chat 실행

        // 표준 입력을 파이프로 리다이렉트
        dup2(pipe_parent_to_child[0], STDIN_FILENO);  // your code here
        // 표준 출력을 파이프로 리다이렉트
        dup2(pipe_child_to_parent[1], STDOUT_FILENO);  // your code here
        
        // 사용하지 않는 파이프 끝 닫기
        close(pipe_parent_to_child[1]);  // your code here
        close(pipe_child_to_parent[0]);  // your code here
        // ai_helper_chat 실행
        execl("./ai_helper_chat_repl", "ai_helper_chat_repl", (char *)NULL);
        perror("execl");
        _exit(127);
    }

    // 부모인 경우 자식에게는 쓰기 파이프 열고, 자식으로부터는 읽기 파이프 열기
    // 나머지는 닫음. 
    close(pipe_parent_to_child[0]);  // your code here
    close(pipe_child_to_parent[1]);  // your code here

    char buf[1024];
    while (1) {
        printf("mini-shell> ");
        fflush(stdout);

        // 사용자 입력 읽기 fgets() 사용, stdin에서 읽기
        if (!fgets(buf, sizeof(buf), stdin))  
            break;
        if (strncmp(buf, "exit", 4) == 0) 
            break;

        // 자식에게 가는 파이프에 입력 전송
        write(pipe_parent_to_child[1], buf, strlen(buf));  // your code here

        // 자식한테서 오는 파이프에 한 줄 응답 읽기, 오기 전까지는 blocking이 됨
        ssize_t n = read(pipe_child_to_parent[0], buf, sizeof(buf) - 1);  // your code here
        if (n > 0) {
            buf[n] = '\0';
            printf("[AI] %s", buf);
        } else {
            break;
        }
    }

    // 파이프 둘다 닫기 
    close(pipe_parent_to_child[1]);  // your code here
    close(pipe_child_to_parent[0]);  // your code here

    // 자식 프로세스 대기
    waitpid(pid, NULL, 0);
    return 0;
}
