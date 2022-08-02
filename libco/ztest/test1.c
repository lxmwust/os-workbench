#include <stdio.h>
#include "../co.h"

void entry(void *arg) {
	printf("%s", (const char *)arg);
	co_yield();
}

int main() {
	struct co *co1 = co_start("co1", entry, "a");
	struct co *co2 = co_start("co2", entry, "b");
	co_wait(co1); // never returns
	co_wait(co2);
}
