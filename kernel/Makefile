NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
endif

include $(AM_HOME)/Makefile
include ../Makefile.lab
image: git

test: git
	@gcc $(shell find src/ -name "*.c" | grep -v "dev" | grep -v "kmt")  \
		$(shell find test/ -name "*.c") \
		-Iframework -Itest -DTEST -lpthread \
		-o build/test
	@build/test
