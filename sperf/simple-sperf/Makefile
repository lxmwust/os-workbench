.PHONY: all test clean

NAME := sperf

all: $(NAME)

$(NAME):
	gcc $(NAME).c -o $@.out
	
test:
	gcc -DLOCAL_MACHINE $(NAME).c -o $(NAME)-debug.out
	$(NAME)-debug.out echo hello

clean:
	rm -f $(NAME).out $(NAME)-debug.out