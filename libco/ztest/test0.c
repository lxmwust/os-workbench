#include <stdio.h>
#include "../co.h"

void entry(void *arg) {
	printf("%s", (const char *)arg);
	co_yield();
}

int main() {
	struct co *co1 = co_start("co1", entry, "a");
	co_wait(co1);
}
