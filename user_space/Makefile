CC = gcc
CFLAGS = -Wall -O2

all: main

main: main.c queue.c
	$(CC) $(CFLAGS) -o main main.c queue.c -lpthread

clean:
	rm -f main

