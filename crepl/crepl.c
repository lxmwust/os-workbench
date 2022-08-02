#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>

static int wrapper_num;
static char *wrapper_func[] = {
	"int ", "__expr_wrapper_", "(){return ", ";}\n",
};
static char wrapper_filec[] = "/tmp/crepl_tmp.c";
static char wrapper_fileso[] = "/tmp/crepl_tmp.so";
static FILE *wrapper_fd;
static char *const gcc_argv[] = {
	"gcc",
	"-fPIC",
	"-shared",
	wrapper_filec,
	"-o",
	wrapper_fileso,
	NULL
};

void compile() {
	if (fork() == 0) {
		execvp("gcc", gcc_argv);
		perror("gcc error");
		exit(-1);
	}
	wait(NULL);
}

int main(int argc, char *argv[]) {
	// empty the file
	wrapper_fd = fopen(wrapper_filec, "w");
	fclose(wrapper_fd);

  static char line[4096];
	void *handle;
	int (*wrapper)();
	char wrapper_buf[100];
	
  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
			// read failed
      break;
    }

		int len = strlen(line);
		if (len >= 3 && !strncmp(line, "int", 3)) {
			// function define
			wrapper_fd = fopen(wrapper_filec, "a");
			fprintf(wrapper_fd, "\n%s\n", line);
			fclose(wrapper_fd);
		}
		else {
			// expression
			wrapper_fd = fopen(wrapper_filec, "a");
			fprintf(wrapper_fd, "%s%s%d%s%s%s",
					wrapper_func[0], wrapper_func[1], wrapper_num, wrapper_func[2], line, wrapper_func[3]);
			fclose(wrapper_fd);

			compile();
			handle = dlopen(wrapper_fileso, RTLD_LAZY);
			if (!handle) {
				printf("handle error\n");
				continue;
			}
			sprintf(wrapper_buf, "%s%d", wrapper_func[1], wrapper_num);
			wrapper = (int (*)()) dlsym(handle, wrapper_buf);
			if (!wrapper) {
				printf("wrapper error\n");
			}
			else {
				printf("%d\n", wrapper());
			}
			wrapper_num++;
			dlclose(handle);
		}
  }
}
