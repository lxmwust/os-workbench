#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <regex.h>

#define TIME_INTERVAL_SECONDS (1)
#define MAX_STRACE_OUTPUT_SIZE (4095)
#define PATH_SPLIT ':'

// strace显示系统调用时间的参数
char *STRACE_SHOW_TIME = "-T", *STRACE_OUTPUT = "-o";

// glibc中定义好的环境变量
extern char **environ;

//管道文件描述符，用来进程间通信
int pipefd[2] = {0};



/*
 * 用来统计系统调用的信息
 */
#define SYSCALL_SIZE	(400)
typedef struct SYSCALL_INFO {
	const char *syscall;
	int syscall_name_size;
	long long time;
} Syscall_Info;
Syscall_Info *syscall_info_sort_by_name[SYSCALL_SIZE] = {NULL}, *syscall_info_sort_by_time[SYSCALL_SIZE] = {NULL};
int syscall_info_number = 0;
long long syscall_time_total = 0;

/*
 * 根据系统调用字符串，查找syscall_info_sort_by_name数组，从而获取字符串数组所在的下标
 */
int syscall_info_find_idx_by_name(const char *name) {
	int left = 0, right = syscall_info_number - 1;
	while(left <= right) {
		int middle = left + (right - left) / 2, cmp = strcmp(syscall_info_sort_by_name[middle]->syscall, name);
		if(cmp == 0) { return middle; }
		else if(cmp < 0) { left = middle + 1; }
		else { right = middle - 1; }
	}
	return -1;
}


/*
 * 根据系统调用，排序名称或者消耗时间
 * 快排
 */
#define syscall_info_sort_by_name_sort() _syscall_info_qsort(syscall_info_sort_by_name, 0, syscall_info_number - 1, syscall_info_sort_by_name_cmp)
#define syscall_info_sort_by_time_sort() _syscall_info_qsort(syscall_info_sort_by_time, 0, syscall_info_number - 1, syscall_info_sort_by_time_cmp)
void _syscall_info_qsort(Syscall_Info **base, int left, int right, int (*cmp)(Syscall_Info **base, int i, int j)) {
	if(left >= right) { return; }

	int leftIndex = left, rightIndex = right + 1;
	Syscall_Info *temp = NULL;

	while(1) {
		while((*cmp)(base, left, ++leftIndex) > 0) {
			if(leftIndex == right) { break; }
		}

		//为了确保正确性，不能为等号，否则rightIndex可能取到left - 1
		while((*cmp)(base, left, --rightIndex) < 0) {;}

		if(leftIndex >= rightIndex) { break; }

		temp = base[leftIndex];
		base[leftIndex] = base[rightIndex];
		base[rightIndex] = temp;
	}


	temp = base[left];
	base[left] = base[rightIndex];
	base[rightIndex] = temp;

	_syscall_info_qsort(base, left, rightIndex - 1, cmp);
	_syscall_info_qsort(base, rightIndex +1, right, cmp);
}


int syscall_info_sort_by_name_cmp(Syscall_Info **base, int i, int j) {
	return strcmp(base[i]->syscall, base[j]->syscall);
}

int syscall_info_sort_by_time_cmp(Syscall_Info **base, int i, int j) {
	//为了确保正确性，如果i == j，则应该返回0，否则qsort的rightIndex可能越界
	if(base[i]->time == base[j]->time) { return 0; }
	else if(base[i]->time > base[j]->time) { return -1; }
	return 1;
}




/*
 * 添加系统调用的相关信息，
 * 如果系统调用存在的话，直接增加其时间即可
 * 如果系统调用不存在的话，新创建
 * 插入完成后，完成相关的排序即可
 */
void syscall_info_insert_and_sort(const char *name, long long time) {

	syscall_time_total += time;

	int syscall_info_idx = 0;
	if((syscall_info_idx = syscall_info_find_idx_by_name(name)) == -1) {
		//此时系统调用信息不能存在，需要创建一个
		Syscall_Info *syscall_info = (Syscall_Info*)malloc(sizeof(Syscall_Info));
		syscall_info->syscall = name;
		syscall_info->syscall_name_size = strlen(name);
		syscall_info->time = time;

		syscall_info_sort_by_name[syscall_info_number] = syscall_info;
		syscall_info_sort_by_time[syscall_info_number++] = syscall_info;


		syscall_info_sort_by_name_sort();
		syscall_info_sort_by_time_sort();
	}else {
		//此时系统调用信息存在，直接修改并排序即可
		syscall_info_sort_by_name[syscall_info_idx]->time += time;
		syscall_info_sort_by_time_sort();
	}
}



/*
 * 将系统调用的统计信息以图形化的形式进行展示
 */
//根据实验指南说明，其为图像中展示的不同的系统调用的个数
#define SYSCALL_INFO_SHOW_SIZE (5)

//设置终端展示时候的窗口高
#define SYSCALL_INFO_WINDOW_HEIGHT (20)
//设置终端展示时候的窗口高
#define SYSCALL_INFO_WINDOW_WIDTH (40)

#define syscall_info_show_format(color) ("\e["#color";37m%s\e[0m")
const char *syscall_info_show_formats[SYSCALL_INFO_SHOW_SIZE] = {syscall_info_show_format(42), syscall_info_show_format(45), syscall_info_show_format(43), syscall_info_show_format(44), syscall_info_show_format(46)};
#define syscall_info_show(idx, str) (fprintf(stderr, syscall_info_show_formats[(idx)], (str)))

#define syscall_info_show_move(opcode) (fprintf(stderr, "\e[1"#opcode))
//将当前光标上移n行，列数不变
void syscall_info_show_move_up(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(A); }
}
//将当前光标下移n行，列数不变
void syscall_info_show_move_down(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(B); }
}
//将当前光标左移n列，行数不变
void syscall_info_show_move_left(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(D); }
}
//将当前光标右移n列，行数不变
void syscall_info_show_move_right(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(C); }
}
//将光标默认移动到第0行，第0列
#define syscall_info_show_position_init() (fprintf(stderr, "\e[0;0H"))



/*
 * 由于每次切割后,记录可用的矩阵的左上坐标即可
 * (row, col)
 * (0,0)			(0, width - 1)
 * _______________________________________
 * |					 |
 * |					 |
 * |_____________________________________|
 * (height - 1, 0)			(height - 1, witdh - 1)
 */
void syscall_info_display() {
	
	//为了使gui界面完整，界面左上角始终从(0， 0)开始
	syscall_info_show_position_init();


	int left_top_row = 0, left_top_col = 0, syscall_info_idx = 0, height = 0, width = 0;
	long long syscall_info_show_time_total = 0;


	//计算需要展示部分的总时间，方便统计每次切分窗口的大小
	for(int i = 0; i < SYSCALL_INFO_SHOW_SIZE && i < syscall_info_number; ++i) { syscall_info_show_time_total += syscall_info_sort_by_time[i]->time; }


	//为了避免留有空白，则最后一部分不需要进行切割，直接有多少切分多少;其余按照比例进行切割即可
	for(; syscall_info_idx + 1 < SYSCALL_INFO_SHOW_SIZE && syscall_info_idx + 1 < syscall_info_number; ++syscall_info_idx) {

		//此时其宽和窗口的宽等宽，高按照比例进行相关的切分
		if(syscall_info_idx & 1) {
			width = SYSCALL_INFO_WINDOW_WIDTH - left_top_col;
			height = (SYSCALL_INFO_WINDOW_HEIGHT - left_top_row) * (syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_info_show_time_total);
		}else {
		//此时高和窗口的高等高，宽按照比例进行切分即可
			height = SYSCALL_INFO_WINDOW_HEIGHT - left_top_row;
			width = (SYSCALL_INFO_WINDOW_WIDTH - left_top_col) * (syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_info_show_time_total);
		}
		syscall_info_show_time_total -= syscall_info_sort_by_time[syscall_info_idx]->time;


		//将[left_top_row, left_top_col] -> [left_top_row + height, left_top_col + width]为对角线的矩形进行上色即可
		int row_end = left_top_row + height, col_end = left_top_col + width;
		for(int row = left_top_row; row < row_end; ++row) {
			for(int col = left_top_col; col < col_end; ++col) { syscall_info_show(syscall_info_idx, " "); }
			syscall_info_show_move_down(1);
			syscall_info_show_move_left(width);
		}
		

		/*
		 * 输出系统调用及其消耗时间占比
		 * 名称位于第一行的最左侧
		 * 系统调用位于下一行的最左侧
		 */
		syscall_info_show_move_up(height);
		syscall_info_show(syscall_info_idx, syscall_info_sort_by_time[syscall_info_idx]->syscall);

		syscall_info_show_move_down(1);
		syscall_info_show_move_left(syscall_info_sort_by_time[syscall_info_idx]->syscall_name_size);

		char percentage[10] = {0};
		sprintf(percentage, "%2.0lf%%", syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_time_total * 100);
		syscall_info_show(syscall_info_idx, percentage);

		//下一个从左下角开始
		if(syscall_info_idx & 1) {
			syscall_info_show_move_down(height - 1);	//即left_top_row + height - (left_top_row + 1)
			syscall_info_show_move_left(strlen(percentage));			//即left_top_col + strlen(percentage) - left_top_col

			left_top_row += height;
		}else {
		//下一个从右上角开始
			syscall_info_show_move_up(1);			//即left_top_row + 1 - left_top_row
			syscall_info_show_move_right(width - 3);	//即left_top_col + width - (left_top_col + strlen(percentage))
			left_top_col += width;
		}

	}


	// 输出最后一个需要展示的系统调用，这里将剩余的矩形全部分配给他即可
	height = SYSCALL_INFO_WINDOW_HEIGHT - left_top_row;
	width = SYSCALL_INFO_WINDOW_WIDTH - left_top_col;

	for(int row = left_top_row; row < SYSCALL_INFO_WINDOW_HEIGHT; ++row) {
		for(int col = left_top_col; col < SYSCALL_INFO_WINDOW_WIDTH; ++col) {syscall_info_show(syscall_info_idx, " "); }
		syscall_info_show_move_down(1);
		syscall_info_show_move_left(width);
	}

	/*
	 * 输出系统调用及其消耗时间占比
	 * 名称位于第一行的最左侧
	 * 系统调用位于下一行的最左侧
	 */
	syscall_info_show_move_up(height);

	syscall_info_show(syscall_info_idx, syscall_info_sort_by_time[syscall_info_idx]->syscall);
	syscall_info_show_move_down(1);
	syscall_info_show_move_left(syscall_info_sort_by_time[syscall_info_idx]->syscall_name_size);

	char percentage[10] = {0};
	sprintf(percentage, "%2.0lf%%", syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_time_total * 100);
	syscall_info_show(syscall_info_idx, percentage);
	

	//为了位置gui界面的完整，这里将光标设置到gui界面的下一行行首开始输出
	syscall_info_show_move_down(SYSCALL_INFO_WINDOW_HEIGHT - left_top_row - 1);	//即SYSCALL_INFO_WINDOW_HEIGHT - (left_top_row + 1)
	syscall_info_show_move_left(left_top_col + strlen(percentage));				//即left_top_col + strlen(percentage) - 0


	//实验指南中，为了区分不同时刻的输出，添加的分界符号
	for(int i = 0; i < 80; ++i) { fprintf(stderr, "%c", '\x00'); }
	fflush(stderr);
}




/*
 * 通过正则匹配解析字符串的信息，从而获取所有的系统调用统计信息，并完成字符串和系统调用统计信息的更新
 * 这里由于可能字符串包含不完整的一行，则保留不完整的部分，并将其移动至行首即可
 */
regex_t regex_syscall, regex_time;
#define re_syscall ("^[^\\(]+")
#define re_time ("[0-9]+\\.[0-9]+>$")
void parse_strace_output_init(void) {
	if(regcomp(&regex_syscall, re_syscall, REG_EXTENDED | REG_NEWLINE)) {
		fprintf(stderr, "regcomp(&regex_syscall, re_syscall, REG_EXTENDED)\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	if(regcomp(&regex_time, re_time, REG_EXTENDED | REG_NEWLINE)) {
		fprintf(stderr, "regcomp(&regex_time, re_time, REG_EXTENDED)\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
}


/*
 * 根据正则匹配时进行解析即可
 * 如果同时匹配到syscall和time，则完成更新后，继续下一次解析
 * 如果有任何一个未匹配，则从最后一次匹配的位置处，将其移动至行首,并返回剩余的字符串长度即可
 */
#define NS_TO_LONGLONG (1000000)
int parse_strace_output(char *buf, int buf_size) {
	if(buf_size == 0) { return 0; }

	double time = 0;
	regmatch_t pmatch[1];
	char *regex_syscall_so = buf, *regex_syscall_eo = buf, *regex_time_so = buf, *regex_time_eo = buf, regex_matched = 0;

	//开始进行匹配即可
	while(1) {
		if(regexec(&regex_syscall, regex_time_eo, sizeof(pmatch) / sizeof(pmatch[0]), pmatch, 0)) { break;}
		regex_syscall_so = regex_time_eo + pmatch[0].rm_so;
		regex_syscall_eo = regex_time_eo + pmatch[0].rm_eo;


		if(regexec(&regex_time, regex_syscall_eo, sizeof(pmatch) / sizeof(pmatch[0]), pmatch, 0)) { break;}
		regex_time_so = regex_syscall_eo + pmatch[0].rm_so;
		regex_time_eo = regex_syscall_eo + pmatch[0].rm_eo;


		regex_matched = 1;


		//截取系统调用名称
		int syscall_name_size = regex_syscall_eo - regex_syscall_so;
		char *syscall = (char*)malloc(sizeof(char) * (1 + syscall_name_size));
		memcpy(syscall, regex_syscall_so, syscall_name_size);
		syscall[syscall_name_size] = 0;

		//截取系统调用的消耗时间
		sscanf(regex_time_so, "%lf", &time);

		//添加该系统调用及其消耗时间统计
		syscall_info_insert_and_sort(syscall, time * NS_TO_LONGLONG);
	}


	//由于更新了系统调用的统计信息，因此更新其gui图像展示
	if(regex_matched) { syscall_info_display(); }


	//由于可能重叠，因此只能使用memmove，而非memcopy
	int remain_size = buf + buf_size - regex_time_eo;
	memmove(buf, regex_time_eo, remain_size);
	buf[remain_size] = 0;

	return remain_size;
}

/*
 * 实际上glibc已经提前解析好环境变量，不需要过多在进行解析
 * 对于参数，只需要添加-T,显示具体的系统调用的时间即可
 */
char **parse_args_environ(int argc, char *argv[]) {
	char **exec_arg = (char**)malloc(sizeof(char*) * (argc + 4));
	assert(exec_arg && argv);


	//exec_arg[0] = STRACE_PATH, exec_arg[1] = STRACE_SHOW_TIME, exec_arg[2] = "-o", exec_arg[3] = /proc/`pid`/fd, exec_arg[4:] = argv[1:]
	//首先直接复制原始的argv即可，之后会用strace的路径替换exec_arg[0],因此没有必要进行复制
	exec_arg[1] = STRACE_SHOW_TIME;
	exec_arg[2] = STRACE_OUTPUT;
	for(int i = 1; i <= argc; ++i) { exec_arg[i + 3] = argv[i]; }

	return exec_arg;
}

/*
 * 子进程部分
 * 其用来执行/path/to/strace -T -o /proc/`pid`/fd command arg
 */
void child(int argc, char *argv[]) {

	char fd_path[20] = {0}, strace_path[MAX_STRACE_OUTPUT_SIZE] = {0}; // Linux中路径长度最大不超过MAX_STRACE_OUTPUT_SIZE字符

	// 首先关闭读管道文件描述符
	close(pipefd[0]);


	// 获取部分参数
	char **exec_arg = parse_args_environ(argc, argv);


	// 获取exec_arg[3], 即当前进程的写管道描述符
	exec_arg[3] = fd_path;
	sprintf(fd_path, "/proc/%d/fd/%d", getpid(), pipefd[1]);


	//开始根据环境变量中的PATH值进行切割和测试，从而获取strace的路径信息和exec_arg[0]
	exec_arg[0] = strace_path;
	int pathBegin = 0, i = 0;
	char *path = getenv("PATH");

	while(path[i]) {
		while(path[i] && path[i] != PATH_SPLIT) { ++i; }

		// 此时path[pathBegin: i - 1]就是待检测的路径
		strncpy(strace_path, path + pathBegin, i - pathBegin);
		// strncpy(strace_path + i - pathBegin, STRACE_EXECUTE, sizeof(STRACE_EXECUTE) +1);

		execve("/bin/strace", exec_arg, environ);	//如果正确执行，则不会返回，并且将strace输出到写管道描述符

		pathBegin = ++i;
	}


	// 如果执行了execve，则理论上不会执行到这里——也就是如果执行到了这里，必然是execve没有正确执行
	fprintf(stderr, "%s\n", "execve() could not find strace");
	fflush(stderr);
	exit(EXIT_FAILURE);
}


#define TIME_INTERVAL_SECONDS (1)
#define MAX_STRACE_OUTPUT_SIZE (4095)

/*
 * 父进程部分
 * 其解析子进程的管道输出，并且以GUI的形式展示输出
 */
void parent(void) {
	char buf[MAX_STRACE_OUTPUT_SIZE + 1] = {0};
	int read_result = 0, buf_available = 0;

	//关闭无用的写管道文件描述符
	close(pipefd[1]);

	//设置读管道文件描述符为非阻塞模式
	if(fcntl(pipefd[0], F_SETFD, fcntl(pipefd[0], F_GETFD) | O_NONBLOCK) == -1) {
		perror("fcntl");
		exit(EXIT_FAILURE);
	}

	//初始化解析字符串中正则匹配相关模式
	parse_strace_output_init();	

	while(1) {
		//可能子进程确实没有输出，或子进程终止，需要辨别这两种情况
		switch(read_result = read(pipefd[0], buf + buf_available, MAX_STRACE_OUTPUT_SIZE - buf_available)) {
			//子进程当前没有输出，但未终止
			case -1:
				break;
			//子进程当前终止
			case 0:
				exit(EXIT_SUCCESS);
				break;
			//此时子进程和父进程正常通过管道进行通信
			default:
				buf[buf_available + read_result] = 0;
				buf_available = parse_strace_output(buf, buf_available + read_result);
		}

		// 节省资源，休眠TIME_INTERVAL_SECONDS秒
		sleep(TIME_INTERVAL_SECONDS);
	}
}



int main(int argc, char *argv[]) {

	//提前创建管道文件描述符，用来之后进程间通信
	if(pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	

	//创建新进程，其中子进程用来执行/path/to/strace -T -o /proc/`pid`/fd
	switch(fork()) {
		case -1:
			perror("fork");
			exit(EXIT_FAILURE);
			break;
		case 0:
			child(argc, argv);
			break;
		default:
			parent();

	}

	/*
	 * 理论上，child由于执行execve，则其不会返回，即不可能执行到这里
	 * parent由于在read系统调用异常时，会直接返回，也不可能执行到这里
	 */
	fprintf(stderr, "%s\n", "sperf wrong return");
	fflush(stderr);
	exit(EXIT_FAILURE);
}