#include "ai_helper.c"
#include <stdio.h>

int main() {
    char response[4096];

    printf("AI 헬퍼 테스트 시작...\n\n");

    // 간단한 질문
    printf("질문: 안녕하세요?\n");
    if (get_ai_summary("안녕하세요?", response, sizeof(response)) == 0) {
        printf("답변: %s\n\n", response);
    } else {
        printf("에러 발생!\n\n");
    }

    // 시스템 정보 요약 테스트
    printf("질문: 시스템 uptime이 12345초, load average가 1.5입니다. 한 문장으로 요약해 주세요.\n");
    if (get_ai_summary("시스템 uptime이 12345초, load average가 1.5입니다. 한 문장으로 요약해 주세요.", 
                        response, sizeof(response)) == 0) {
        printf("답변: %s\n\n", response);
    } else {
        printf("에러 발생!\n\n");
    }

    return 0;
}