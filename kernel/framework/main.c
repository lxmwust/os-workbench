#include <kernel.h>
#include <klib.h>

int main() {
  ioe_init();
	os->init();
	/* cte_init(os->trap); */
	mpe_init(os->run);
  return 1;
}
