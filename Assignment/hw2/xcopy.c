// POSIX 확장 기능을 활성화 (struct timespec, utimensat, st_atim/st_mtim 등을 사용하기 위함)
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>    // printf, fprintf, perror
#include <stdlib.h>   // exit, EXIT_FAILURE
#include <string.h>   // strcmp, strncpy
#include <sys/stat.h> // stat, lstat, S_ISDIR, S_ISREG 등 파일 상태 및 타입 확인
#include <dirent.h>   // opendir, readdir, closedir
#include <unistd.h>   // getopt, read, write, chmod
#include <errno.h>    // errno, EEXIST
#include <libgen.h>   // basename (경로에서 파일 이름만 추출)
#include <fcntl.h>    // open, O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, AT_FDCWD
#include <limits.h>   // PATH_MAX (경로 최대 길이)
#include <time.h>     // struct timespec (시간 구조체)

#define BUFFER_SIZE 4096 // 파일 복사 시 사용할 버퍼 크기

// 전역 플래그 (옵션 처리)
int recursive_flag = 0; // -r : 재귀적 복사 플래그
int verbose_flag = 0;   // -v : 상세 출력 플래그
int preserve_flag = 0;  // -p : 권한 및 시간 보존 플래그

/**
 * 단일 파일을 복사하는 함수.
 * @param source 원본 파일 경로
 * @param destination 대상 파일 경로
 * @param mode 원본 파일의 권한 (퍼미션)
 * @param atime 원본 파일의 접근 시간
 * @param mtime 원본 파일의 수정 시간
 * @return 성공 시 0, 실패 시 -1
 */
int copy_file(const char *source, const char *destination, mode_t mode, struct timespec atime, struct timespec mtime) {
    int src_fd, dest_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[BUFFER_SIZE];

    // 1. 원본 파일 열기 (읽기 전용) - 간결한 조건 검사 스타일 적용
    if ((src_fd = open(source, O_RDONLY)) < 0) {
        perror("open source file");
        return -1;
    }

    // 2. 대상 파일 열기 (쓰기 전용, 없으면 생성, 있으면 내용 잘라내기(O_TRUNC) = 덮어쓰기)
    // -p 옵션이 없으면 기본 권한 0644 사용, 있으면 원본 권한(mode) 사용
    // 간결한 조건 검사 스타일 적용
    if ((dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, preserve_flag ? mode : (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) < 0) {
        perror("open destination file");
        close(src_fd);
        return -1;
    }

    // -v 옵션 처리: 상세 출력
    if (verbose_flag) {
        printf("복사 중: %s -> %s\n", source, destination);
    }

    // 3. 파일 내용 복사
    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write destination file");
            close(src_fd);
            close(dest_fd);
            return -1;
        }
    }

    if (bytes_read == -1) {
        perror("read source file");
        close(src_fd);
        close(dest_fd);
        return -1;
    }

    // 4. 파일 닫기
    if (close(src_fd) == -1) { perror("close source file"); }
    if (close(dest_fd) == -1) { perror("close destination file"); }


    // 5. -p 옵션 처리: 권한 및 타임스탬프 보존
    if (preserve_flag) {
        // 권한 보존: 파일 경로를 사용하여 권한 설정
        if (chmod(destination, mode) == -1) {
            perror("chmod destination file");
        }
        
        // 타임스탬프 보존: 접근 시간(atime)과 수정 시간(mtime) 설정
        struct timespec times[2];
        times[0] = atime; // 접근 시간 (Access Time)
        times[1] = mtime; // 수정 시간 (Modification Time)
        
        // utimensat 시스템 콜을 사용하여 시간 정보 업데이트
        if (utimensat(AT_FDCWD, destination, times, 0) == -1) {
            perror("utimensat destination file");
        }
    }

    return 0;
}


/**
 * 디렉토리를 재귀적으로 복사하는 함수 (-r 옵션 처리).
 * @param source 원본 디렉토리 경로
 * @param destination 대상 디렉토리 경로
 * @return 성공 시 0, 실패 시 -1
 */
int copy_dir(const char *source, const char *destination) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];

    // 1. 대상 디렉토리 생성
    // 이미 존재하는 경우 (EEXIST)는 무시하고 계속 진행 (덮어쓰기 로직)
    if (mkdir(destination, 0755) == -1 && errno != EEXIST) {
        perror("mkdir destination directory");
        return -1;
    }

    // 2. 원본 디렉토리 열기
    dir = opendir(source);
    if (dir == NULL) {
        perror("opendir source directory");
        return -1;
    }

    // 3. 디렉토리 항목 순회
    while ((entry = readdir(dir)) != NULL) {
        // . (현재 디렉토리)와 .. (부모 디렉토리) 항목은 건너뛰기
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 새 경로 구성 (PATH_MAX 초과 검사 포함)
        if (snprintf(source_path, PATH_MAX, "%s/%s", source, entry->d_name) >= PATH_MAX ||
            snprintf(dest_path, PATH_MAX, "%s/%s", destination, entry->d_name) >= PATH_MAX) {
            fprintf(stderr, "xcopy: path name too long for '%s'\n", entry->d_name);
            continue;
        }

        // 4. lstat 사용: 심볼릭 링크 자체의 정보를 얻어 파일 타입 분기 처리
        // lstat 결과가 0보다 작으면 (에러) 해당 항목 건너뛰기
        if (lstat(source_path, &st) < 0) {
            perror("lstat source_path");
            continue; 
        }

        if (S_ISDIR(st.st_mode)) { // 디렉토리인 경우
            if (recursive_flag) {
                // -r 옵션이 설정된 경우에만 재귀적으로 복사
                if (copy_dir(source_path, dest_path) == -1) {
                    closedir(dir);
                    return -1;
                }
            } else {
                // -r 옵션이 없는 경우, 디렉토리는 무시하고 경고 출력
                fprintf(stderr, "xcopy: omitting directory '%s' (use -r to copy recursively)\n", source_path);
            }
        } else if (S_ISREG(st.st_mode)) { // 일반 파일인 경우
            // copy_file 함수를 호출하여 복사 실행
            if (copy_file(source_path, dest_path, st.st_mode, st.st_atim, st.st_mtim) == -1) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISLNK(st.st_mode)) { // 심볼릭 링크인 경우 (무시)
            fprintf(stderr, "xcopy: ignoring symbolic link '%s'\n", source_path);
            continue;
        } else { // 기타 파일 타입 (FIFO, socket 등)
            fprintf(stderr, "xcopy: skipping unknown file type for '%s'\n", source_path);
            continue;
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;
    char *source_arg = NULL;
    char *target_arg = NULL;

    // 명령줄 옵션 파싱 (-r, -v, -p)
    while ((opt = getopt(argc, argv, "rvp")) != -1) {
        switch (opt) {
            case 'r':
                recursive_flag = 1;
                break;
            case 'v':
                verbose_flag = 1;
                break;
            case 'p':
                preserve_flag = 1;
                break;
            default:
                // 알 수 없는 옵션이나 옵션 인자가 누락된 경우 사용법 출력
                fprintf(stderr, "Usage: %s [-r] [-v] [-p] SOURCE TARGET\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // SOURCE와 TARGET 인자 확인
    if (argc - optind < 2) { 
        fprintf(stderr, "Usage: %s [-r] [-v] [-p] SOURCE TARGET\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    source_arg = argv[optind];
    target_arg = argv[optind + 1];

    struct stat source_stat;

    // 1. 원본 파일/디렉토리 정보 확인 (심볼릭 링크 정보는 lstat으로 얻음) - 간결한 조건 검사 스타일 적용
    if (lstat(source_arg, &source_stat) < 0) {
        perror("lstat source");
        exit(EXIT_FAILURE);
    }

    // 2. 원본이 심볼릭 링크인 경우 무시하고 종료
    if (S_ISLNK(source_stat.st_mode)) {
        fprintf(stderr, "xcopy: ignoring symbolic link '%s' as source\n", source_arg);
        exit(EXIT_FAILURE);
    }

    // 3. Source가 디렉토리인 경우
    if (S_ISDIR(source_stat.st_mode)) { 
        if (!recursive_flag) {
            fprintf(stderr, "xcopy: '%s' is a directory, use -r to copy recursively\n", source_arg);
            exit(EXIT_FAILURE);
        }

        struct stat target_stat;
        char final_target_path[PATH_MAX];

        // 대상 경로 분석
        if (lstat(target_arg, &target_stat) == 0) {
            if (S_ISDIR(target_stat.st_mode)) {
                // 대상이 이미 존재하는 디렉토리: source를 그 안에 복사 (예: A -> B/A)
                if (snprintf(final_target_path, PATH_MAX, "%s/%s", target_arg, basename(source_arg)) >= PATH_MAX) {
                    fprintf(stderr, "xcopy: final path name too long\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                // 대상이 존재하지만 디렉토리가 아님: 에러
                fprintf(stderr, "xcopy: cannot overwrite non-directory '%s' with directory '%s'\n", target_arg, source_arg);
                exit(EXIT_FAILURE);
            }
        } else {
            // 대상이 존재하지 않거나 접근 불가 (ENOENT): 새 디렉토리 이름으로 간주 (예: A -> B)
            strncpy(final_target_path, target_arg, PATH_MAX - 1);
            final_target_path[PATH_MAX - 1] = '\0';
        }

        // 디렉토리 복사 시작
        if (copy_dir(source_arg, final_target_path) == -1) {
            exit(EXIT_FAILURE);
        }

    // 4. Source가 일반 파일인 경우
    } else if (S_ISREG(source_stat.st_mode)) { 
        struct stat target_stat;
        char final_target_path[PATH_MAX];

        // 대상 경로 분석
        if (lstat(target_arg, &target_stat) == 0 && S_ISDIR(target_stat.st_mode)) {
            // 대상이 존재하는 디렉토리: source 파일을 그 안에 복사 (예: a.txt -> B/a.txt)
            if (snprintf(final_target_path, PATH_MAX, "%s/%s", target_arg, basename(source_arg)) >= PATH_MAX) {
                fprintf(stderr, "xcopy: final path name too long\n");
                exit(EXIT_FAILURE);
            }
        } else {
            // 대상이 존재하지 않거나 파일인 경우 (덮어쓰기): target_arg를 최종 경로로 사용 (예: a.txt -> b.txt)
            strncpy(final_target_path, target_arg, PATH_MAX - 1);
            final_target_path[PATH_MAX - 1] = '\0';
        }

        // 파일 복사 시작
        if (copy_file(source_arg, final_target_path, source_stat.st_mode, source_stat.st_atim, source_stat.st_mtim) == -1) {
            exit(EXIT_FAILURE);
        }

    // 5. 기타 파일 타입 (FIFO, socket 등)
    } else { 
        fprintf(stderr, "xcopy: skipping unknown source file type for '%s'\n", source_arg);
        exit(EXIT_FAILURE);
    }

    return 0;
}