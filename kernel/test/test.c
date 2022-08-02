#include <kernel.h>
#include <stdio.h>
#include <thread.h>

static void entry(int tid) {
	void *addr = pmm->alloc(1 << 18);
	pmm->free(addr);
	addr = pmm->alloc(1 << 17);
	pmm->free(addr);
	/* addr = pmm->alloc(128); */
	/* void *addr16 = pmm->alloc(16); */
	/* pmm->free(addr); */
	/* pmm->free(addr16); */
}
static void finish() { printf("End\n"); }

int main() {
	pmm->init();
	for (int i = 0; i < 1; i++) {
		create(entry);
	}
	
	join(finish);
}
