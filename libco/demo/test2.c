#include <stdio.h>
#include "../co.h"

int g_cnt = 0;

void entry(void *arg) {
	for (int i = 0; i < 100; i ++ ) {
		printf("%s%d ", (const char *)arg, g_cnt);
		g_cnt ++ ;
		co_yield();
	}
}

int main() {
	struct co *co1 = co_start("co1", entry, "a");
	struct co *co2 = co_start("co2", entry, "b");
	co_wait(co1); // never returns
	co_wait(co2);
}
