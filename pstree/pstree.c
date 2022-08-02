#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#define VERSION "Version: 0.0.1"

struct process_state {
	pid_t pid;
	char name[30];
	char state;
	pid_t ppid;
};

struct node {
	struct process_state ps;
	struct node *next;
	struct node *child;
};
struct node *node_malloc();
int node_insert(struct node *sn, struct node *fn);
void node_free(struct node *fn);

static struct option longopts[] = {
	{ "show-pids",		0, NULL, 'p' },
	{ "numeric-sort",	0, NULL, 'n' },
	{ "version",		0, NULL, 'V' },
	{ 0,				0, 0,	 0   }
};

int isNumber(char *str);
void buildProcessTree(struct node *head);
void printProcessTree(struct node *head, int depth, int type);
void printProcess(struct node *node, int type);
void printVersion();

int main(int argc, char *argv[]) {
	char opt;
	int option_index;
	struct node *head;

	head = node_malloc();
	buildProcessTree(head);

	if (argc == 1) {
		printProcessTree(head, 0, 0);
	}
	else {
		while ((opt = getopt_long(argc, argv, "pnV", longopts, &option_index)) != -1) {
			switch (opt) {
			case 'p':
				printProcessTree(head, 0, 1);
				break;
			case 'n':
				printProcessTree(head, 0, 1);
				break;
			case 'V':
				printf("%s\n", VERSION);
				break;
			default:
				printf("pstree\n");
				printf("A convenient tool to show running processes as a tree.\n");
				printf("Use -p -n -V to see more");
				break;
			}
		}
	}

	node_free(head);

	return 0;
}

int isNumber(char *str) {
	for (char *c = str; *c != '\0'; c ++ )
		if (isdigit(*c) == 0)
			return 0;
	return 1;
}

struct node *node_malloc() {
	struct node *n = (struct node *)malloc(sizeof(struct node));

	n->ps = (struct process_state){ 0, { }, 0 };
	n->child = NULL;
	n->next = NULL;

	return n;
}

int node_insert(struct node *sn, struct node *fn) {
	struct node *p;
	
	if (sn->ps.ppid == fn->ps.pid) {
		if (fn->child == NULL)
			fn->child = sn;
		else {
			for (p = fn->child; p->next != NULL; p = p->next)
				;
			p->next = sn;
		}
		return 1;
	}
	else {
		for (p = fn->child; p != NULL; p = p->next)
			if (node_insert(sn, p)) {
				return 1;
			}
	}

	return 0;
}

void node_free(struct node *fn) {
	struct node *p = fn->child;

	if (p != NULL)
		for ( ; p != NULL; ) {
			struct node *tmp = p;
			p = p->next;
			node_free(tmp);
		}
	
	free(fn);
}

void buildProcessTree(struct node *head) {
	DIR *d;
	struct dirent *dir;
	FILE *f;
	char path[100] = "/proc/";
	int pathLen = 6;

	d = opendir(path);
	if (!d) {
		fprintf(stderr, "Directory %s does not exist\n", path);
		exit(1);
	}
	else {
		while ((dir = readdir(d)) != NULL) {
			if (isNumber(dir->d_name)) {
				strcpy(path + pathLen, dir->d_name);
				strcat(path, "/stat");

				f = fopen(path, "r");
				if (!f) {
					fprintf(stderr, "Open file %s failed\n", path);
					continue;
				}
				else {
					struct node *node = node_malloc();
					fscanf(f, "%d (%[^)]) %c %d",
							&node->ps.pid, node->ps.name, &node->ps.state, &node->ps.ppid);
					node_insert(node, head);

					/* printf("%d %s %d\n", */
							/* node->ps.pid, node->ps.name, node->ps.ppid); */

					fclose(f);
				}
			}
		}
		closedir(d);
	}
}

void printProcessTree(struct node *node, int depth, int type) {
	if (node->ps.pid != 0) {
		printProcess(node, type);
		depth ++ ;
	}

	for (struct node *p = node->child; p != NULL; p = p->next) {
		for (int i = 0; i < depth; i ++ ) {
			printf("  ");
		}
		printProcessTree(p, depth, type);
	}
}

void printProcess(struct node *node, int type) {
	if (type == 0) {
		fprintf(stdout, "%s\n", node->ps.name);
	}
	else if (type == 1) {
		fprintf(stdout, "%s(%d)\n", node->ps.name, node->ps.pid);
	}
}

void printVersion() {
	fprintf(stdout, VERSION);
}
