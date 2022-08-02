#include "thread.h"

#ifdef LOCAL_MACHINE
#define debug(...)                                        \
  printf("In file [%s] line [%d]: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__)
#else
#define debug(msg)
#endif

void Ta() {
  int i = 0;
  while (i < 5) {
    debug("a\n");
    i++;
    usleep(100000);
  }
}
void Tb() {
  int i = 0;
  while (i < 5) {
    debug("b\n");
    i++;
    usleep(10000);
  }
}
void Tc() {
  int i = 0;
  while (i < 5) {
    debug("c\n")
        i++;
    usleep(1000);
  }
}

int main() {
  create(Ta);
  create(Tb);
  create(Tc);
  debug("running...\n");
  join();
  debug("end...\n");
}

// gcc a.c -lpthread -DLOCAL_MACHINE && ./a.out
/*
In file [a.c] line [14]: a
In file [a.c] line [40]: running...
In file [a.c] line [22]: b
In file [a.c] line [30]: c
In file [a.c] line [30]: c
In file [a.c] line [30]: c
In file [a.c] line [30]: c
In file [a.c] line [30]: c
In file [a.c] line [22]: b
In file [a.c] line [22]: b
In file [a.c] line [22]: b
In file [a.c] line [22]: b
In file [a.c] line [14]: a
In file [a.c] line [14]: a
In file [a.c] line [14]: a
In file [a.c] line [14]: a
In file [a.c] line [42]: end...
*/