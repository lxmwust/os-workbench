#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

int main() {
    int n = 0;
    jmp_buf buf;
    setjmp(buf);
    printf("Hello %d\n", n);
    longjmp(buf, n++);
}
