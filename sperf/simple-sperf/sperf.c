#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

#ifdef LOCAL_MACHINE
	#define debug(format, arg...) printf(format, ##arg)
	#define debug_info(format, ...) printf("\033[1m\033[45;33m Info:[%s:%s(%d)]: \033[0m" format "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
	#define debug(format, arg...);
	#define debug_info(format, ...)
#endif

#define zassert(x, s) \
	do { if ((x) == 0) { printf("%s\n", s); assert((x)); } } while (0)

char buf[BUFSIZ];
struct info {
	char name[100];
	double time;
	struct info *next;
};
struct info *syscall_info = NULL;
void add(double time, char *name) {
	for (struct info *i = syscall_info; i != NULL; i = i->next) {
		if (strcmp(i->name, name) == 0) {
			i->time += time;
			return;
		}
	}
	struct info *si = (struct info *)malloc(sizeof(struct info));
	strcpy(si->name, name);
	si->time = time;
	si->next = syscall_info;
	/* printf("-- %lf %s\n", si->time, si->name); */
	debug_info("-- %lf %s\n", si->time, si->name);

	syscall_info = si;
}

int main(int argc, char *argv[], char *envp[]) {
	zassert(argc >= 2, "need at least one argument");
	
	debug_info("Test Begin.");
	for (int i = 0; i < argc; i++) {
		debug_info("argv[%d]:%s", i, argv[i]);
	}

	char *strace_argv[] = { "strace", "-r", argv[1], NULL };
	int pipefd[2];
	int pid;

	zassert(pipe(pipefd) == 0, "create pipe failed");
	
	pid = fork();
	if (pid == 0) {
		// 关闭stderr
		close(2);
		// 将stderr接到管道的输入，stderr将输出到管道的输入
		dup2(pipefd[1], 2);
		close(pipefd[1]);
		// 关闭stdout（比如ls，会输出到stdout）
		close(1);
		// 关闭管道输出，用不到
		close(pipefd[0]);

		execve("/bin/strace", strace_argv, envp);
		zassert(0, "execve failed");
	}
	else {
		// 关掉父进程的管道输入，不然管道输出会阻塞
		close(pipefd[1]);
		double time;
		char name[100];

		while (fgets(buf, sizeof(buf), fdopen(pipefd[0], "r"))) {
			/* printf("%s", buf); */
			sscanf(buf, "%lf %s", &time, name);
			if (name[0] == '+') { continue; }

			for (int i = 0; name[i]; i ++ ) {
				if (name[i] == '(') {
					name[i] = '\0';
					break;
				}
			}
			add(time, name);
			/* printf("%lf %s\n", time, name); */
		}
	}

	double total = 0, others = 0;
	struct info syscall_sort[5] = { };

	for (struct info *i = syscall_info; i != NULL; i = i->next) {
		for (int j = 0; j < 4; j++) {
			if (syscall_sort[j].time < i->time) {
				for (int k = 2; k >= j; k--) {
					syscall_sort[k + 1] = syscall_sort[k];
				}
				syscall_sort[j] = *i;
				break;
			}
		}
		total += i->time;
		/* printf("%s (%lf)\n", i->time, i->name); */
		/* fflush(stdout); */
	}

	others = total;
	for (int i = 0; i < 5; i++) {
		if (syscall_sort[i].time > 0) {
			printf("%s (%lf%%)\n", syscall_sort[i].name, syscall_sort[i].time / total);
			fflush(stdout);
			others -= syscall_sort[i].time;
		}
		else {
			printf("%s (%lf%%)\n", "others", others / total);
			fflush(stdout);
		}
	}

	for (struct info *i = syscall_info; i != NULL; ) {
		struct info *j = i;
		i = i->next;
		free(j);
	}
}
