SHELL:=/bin/bash
override CFLAGS += -Wall -Wextra -pedantic -g
LIBRARIES = -lssh2 -lpthread
SOURCEFILES = $(wildcard *.c *.h)

build: $(SOURCEFILES)
	gcc $(CFLAGS) ghostssh.c -I. $(LIBRARIES)  -o ghostssh

static: $(SOURCEFILES)
	read -s -p "SSH Password: " PASSWORD; \
	gcc $(CFLAGS) ghostssh.c -DGHOSTSSH_USERNAME=\"$$USERNAME\" -DGHOSTSSH_PASSWORD=\"$$PASSWORD\" -I. $(LIBRARIES)  -o ghostssh
	chmod 100 ghostssh
