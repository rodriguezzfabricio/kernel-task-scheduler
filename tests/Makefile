CC = gcc
CFLAGS = -Wall -O2 -I../user_space

all: kernel_tests user_tests

kernel_tests: kernel_tests.c
	$(CC) $(CFLAGS) -o kernel_tests kernel_tests.c

user_tests: user_tests.c ../user_space/queue.c
	$(CC) $(CFLAGS) -o user_tests user_tests.c ../user_space/queue.c -lpthread

clean:
	rm -f kernel_tests user_tests

run1: kernel_tests
	$(MAKE) -C ../kernel_space
	sudo insmod ../kernel_space/kqueue.ko
	./kernel_tests
	sudo rmmod kqueue

run2: user_tests
	./user_tests

