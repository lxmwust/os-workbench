# libco

## 问题记录

### 问题1: 编译 libco-32.so

```bash
gcc -fPIC -shared -m32 -U_FORTIFY_SOURCE -O1 -std=gnu11 -ggdb -Wall -Werror -Wno-unused-result -Wno-unused-value -Wno-unused-variable ./co.c -o libco-32.so 
In file included from /usr/include/assert.h:35,
                 from ./co.c:3:
/usr/include/features.h:461:12: fatal error: sys/cdefs.h: No such file or directory
  461 | #  include <sys/cdefs.h>
      |            ^~~~~~~~~~~~~
compilation terminated.
make: *** [../Makefile:28: libco-32.so] Error 1
```

解决：

```bash
$ sudo apt install gcc-multilib
```
