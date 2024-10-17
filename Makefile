CC = cc
CFLAGS = -std=c2x -Wall -Wextra -Wpedantic -Werror -ggdb
LDFLAGS = -ledit

all: vm

vm: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean: 
	rm -f vm
