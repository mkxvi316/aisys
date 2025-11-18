
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define API_URL "http://localhost:11434/api/generate"
#define MODEL_NAME_QWEN3 "qwen3:0.6b" // Qwen3, r 모델이라 좀 느림
#define MODEL_NAME_GEMMA3 "gemma3:270m" // Gemma3, 270m 밖에 안해서 빠름. 우리 AISYS 실습에서는 이걸 사용할 것입니다. 
#define MODEL_NAME_GPT_OSS "gpt-oss:20b" // gpu 좋은 것 있으신 분은 gpt-oss:20b 혹은 gpt-oss:120b 써보세요.


// libcurl4 설치
// sudo apt-get install libcurl4-openssl-dev
// 컴파일시 라이브러리 링크 옵션 추가 필요:
// gcc -o test_ai_helper test_ai_helper.c -lcurl
// 혹은 ./vscode/tasks.json 에서 링크 옵션 추가(args: ["-lcurl"])

// libcurl 응답 저장용 구조체
typedef struct {
    char *data;
    size_t size;
} MemoryStruct;

// libcurl 콜백 함수
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;
    
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

int get_ai_summary(const char *prompt, char *response, size_t response_size) {
    if (!prompt || !response || response_size == 0) return -1;
    
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    
    // JSON 이스케이프 (간단히 " -> ')
    char escaped_prompt[8192];
    size_t j = 0;
    for (size_t i = 0; prompt[i] && j < sizeof(escaped_prompt) - 1; i++) {
        if (prompt[i] == '"') {
            escaped_prompt[j++] = '\'';
        } else if (prompt[i] == '\n') {
            if (j < sizeof(escaped_prompt) - 2) {
                escaped_prompt[j++] = '\\';
                escaped_prompt[j++] = 'n';
            }
        } else {
            escaped_prompt[j++] = prompt[i];
        }
    }
    escaped_prompt[j] = '\0';
    
    // JSON 페이로드 생성
    char json_data[16384];
    snprintf(json_data, sizeof(json_data),
        "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}",
        MODEL_NAME_GEMMA3, escaped_prompt);

    // 메모리 버퍼 초기화
    MemoryStruct chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;
    
    // libcurl 설정
    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // API 호출
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    // JSON 파싱: "response":" 찾기
    const char *start = strstr(chunk.data, "\"response\":\"");
    if (!start) {
        free(chunk.data);
        return -1;
    }
    start += 12;
    
    // 이스케이프 처리하며 복사
    size_t idx = 0;
    while (*start && idx < response_size - 1) {
        if (*start == '"' && (start == chunk.data + 12 || *(start - 1) != '\\')) {
            break;
        }
        
        if (*start == '\\') {
            start++;
            if (*start == 'u') {
                start += 5; // \uXXXX 건너뛰기
                continue;
            }
            switch (*start) {
                case 'n': response[idx++] = '\n'; break;
                case 't': response[idx++] = '\t'; break;
                case 'r': response[idx++] = '\r'; break;
                case '\\': response[idx++] = '\\'; break;
                case '"': response[idx++] = '"'; break;
                default: response[idx++] = *start; break;
            }
        } else {
            response[idx++] = *start;
        }
        start++;
    }
    response[idx] = '\0';
    
    free(chunk.data);
    return 0;
}
