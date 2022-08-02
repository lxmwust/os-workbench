#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RPATH "/proc/"
#define SPATH "/stat"
#define STPATH "/status"
#define SN 100
#define LN 10000

struct Proc {
  unsigned int pid; // 线程号
  char comm[LN];
  unsigned char state;
  unsigned int ppid;       // 父进程号
  char name[SN];           // 进程名
  int ccnt;                // 子进程数量
  struct Proc *chrilden[]; // 子进/线程
};

struct Proc *ProcList[];
int ProcN = 0;

void set_proc(char *pid, struct Proc *proc) {

  char stat_path[SN] = {};
  char status_path[SN] = {};

  strcat(stat_path, RPATH);
  strcat(stat_path, pid);
  strcat(stat_path, SPATH);

  strcat(status_path, RPATH);
  strcat(status_path, pid);
  strcat(status_path, STPATH);

  FILE *fp = fopen(stat_path, "r");

  if (fp) {
    rewind(fp);
    fscanf(fp, "%d %s %c %d", &proc->pid, proc->comm, &proc->state,
           &proc->ppid);
    fclose(fp);
  } else {
    // 错误处理
  }

  fp = fopen(status_path, "r");
  char tmp[33];
  if (fp) {
    rewind(fp);
    fscanf(fp, "%s %s", tmp, proc->name);
    fclose(fp);
  } else {
    // 错误处理
  }
  proc->ccnt = 0;
}

void get_procs() {
  DIR *dp;
  struct dirent *dirp;
  char dirname[SN] = RPATH;
  int i = 1;

  if ((dp = opendir(dirname)) == NULL)
    printf("Can't open %s", dirname);

  while ((dirp = readdir(dp)) != NULL) {
    if (dirp->d_type ==
        4) { // d_type表示类型，4表示目录，8表示普通文件，0表示未知设备
      if (48 <= dirp->d_name[0] && dirp->d_name[0] <= 57) { // 数字开头

        struct Proc *proc = (struct Proc *)malloc(sizeof(struct Proc));
        set_proc(dirp->d_name, proc);

        ProcList[i] = proc;
        i++;
      }
    }
  }
  ProcN = i;
  closedir(dp);
}

void gen_procs_tree(struct Proc *root) {

  printf("ProcN: %d\n", ProcN);
  for (int i = 1; i < ProcN; i++) {
    struct Proc *cp = ProcList[i];

    printf("%-15s %-15d %-15d %-15p\n", cp->name, cp->pid, cp->ppid, cp);

    for (int j = 0; j < ProcN; j++) {
      struct Proc *pp = ProcList[j];
      if (cp->ppid == pp->pid) {
        pp->chrilden[pp->ccnt] = cp;
        pp->ccnt++;
      }
    }
  }
}

void show_tree(struct Proc *node, int before_len) {
  printf("--%s(%d)", node->name, node->pid);
  before_len += strlen(node->name) + 7; // 调整这里可以调整对齐程度
  for (int i = 0; i < node->ccnt; i++) {

    if (i == 0) {
      printf("─┬─");
    } else if (i == node->ccnt - 1) {
      for (int j = 0; j < before_len; j++)
        printf(" ");
      printf("└─");
    } else {
      for (int j = 0; j < before_len; j++)
        printf(" ");
      printf("├─");
    }

    show_tree(node->chrilden[i], before_len);
    printf("\n");
  }
}

int main(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
    printf("argv[%d] = %s\n", i, argv[i]);
  }
  assert(!argv[argc]);

  struct Proc *root = (struct Proc *)malloc(sizeof(struct Proc));

  strcpy(root->name, "root");
  root->pid = 0;
  root->ccnt = 0;
  ProcList[0] = root;

  get_procs();

  gen_procs_tree(root);

  show_tree(root, 0);

  return 0;
}

// 未完成工作：
// 1. -p 内容，线程的没写，在/proc/xxx/task 下面
// 2. -n 排序的没有写，写一个冒泡排序即可

// 效果
// --root(0)─┬─--docker-init(1)─┬─--sleep(7)
//                              ├─--dockerd(29)─┬─--containerd(114)

//                              ├─--sshd(61)
//                              └─--cpptools-srv(6059)

//            ├─--sh(786)
//            ├─--sh(836)
//            ├─--sh(1284)─┬─--node(1291)─┬─--node(1100002560)
//                                ├─--node(2570)─┬─--cpptools(3851)
//                                           └─--node(18997)

//                                └─--node(1100184864)

//            ├─--node(1618)
//            ├─--node(1630)
//            ├─--sh(1662)
//            ├─--sh(1745)
//            ├─--sh(2212)─┬─--node(2220)─┬─--node(2249)

//            ├─--sh(2606)
//            ├─--sh(2681)
//            ├─--sh(3210)─┬─--node(3217)─┬─--node(3239)

//            ├─--sh(4172)
//            ├─--sh(4237)
//            ├─--sh(4845)─┬─--node(4852)─┬─--node(4875)

//            ├─--node(4873)
//            └─--node(4899)
