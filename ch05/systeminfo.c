#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/utsname.h>
#include <string.h>

int main(void) {
	struct timespec start, end;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
		perror("clock_gettime(start)");
		return 1;
	}

	// 사용자 정보
	char *login = getlogin();
	if (!login) {
		fprintf(stderr, "getlogin() failed\n");		
	}
	struct passwd *pw = getpwnam(login);
	if (pw) {
		printf("[User Info]\n");
		printf("user: %s, uid=%d, gid=%d, home=%s, shell=%s\n",
			   pw->pw_name, pw->pw_uid, pw->pw_gid, pw->pw_dir, pw->pw_shell);
	} else {
		fprintf(stderr, "getpwnam() failed for %s\n", login);
	}

	// 그룹 정보, linux는 getgroups()에 주 그룹도 포함됨. 
	int ngroups = getgroups(0, NULL);
	if (ngroups < 0) {
		perror("getgroups(count)");
		ngroups = 0;
	}
	gid_t groups[ngroups > 0 ? ngroups : 1];
	if (ngroups > 0 && getgroups(ngroups, groups) < 0) {
		perror("getgroups(list)");
		ngroups = 0;
	}

	printf("\n[Group Info]\n");
	for (int i = 0; i < ngroups; i++) {
		struct group *gr = getgrgid(groups[i]);
		if (gr) printf("%s(%d) ", gr->gr_name, groups[i]);
		else printf("?(%d) ", groups[i]);
	}
	printf("\n");

	// 로그인 정보
	printf("\n[Login Info]\nlogin name: %s\n", login);

	// 호스트 정보
	char hostname[256] = {0};
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		perror("gethostname");
		strcpy(hostname, "unknown");
	}
	struct utsname uts;
	if (uname(&uts) != 0) {
		perror("uname");
		memset(&uts, 0, sizeof(uts));
	}
	printf("\n[Host Info]\n");
	printf("hostname: %s\n", hostname);
	printf("uname: %s %s %s %s\n",
		   uts.sysname[0] ? uts.sysname : "?",
		   uts.nodename[0] ? uts.nodename : "?",
		   uts.release[0] ? uts.release : "?",
		   uts.machine[0] ? uts.machine : "?");

	// 시간 정보
	time_t now = time(NULL);
	if (now == (time_t)-1) { // -1 대신에 time_t-1를 사용한 이유? time_t 가 long/long long 등 다양한 타입이 될 수 있음.
		perror("time");
		now = 0;
	}
	char buf[64] = {0};
	struct tm *lt = localtime(&now);  //
	if (!lt) {
		perror("localtime");
		strncpy(buf, "unknown", sizeof(buf)-1);
	} else {
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
	}
	printf("\n[Time Info]\nepoch: %ld\nlocaltime: %s\n", now, buf);

	// 실행 시간
	if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {        // 암기
		perror("clock_gettime(end)");
		return 1;
	}
	double elapsed = (end.tv_sec - start.tv_sec) +
					 (end.tv_nsec - start.tv_nsec) / 1e9;
	printf("elapsed: %.9f sec\n", elapsed);

	return 0;
}
