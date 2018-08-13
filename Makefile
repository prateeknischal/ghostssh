override CFLAGS += -std=c99 -Wall -Wextra -pedantic -g
LIBRARIES = -lssh2 -lpthread
SOURCEFILES = $(wildcard *.c *.h)

build: $(SOURCEFILES)
	gcc $(CFLAGS) -I. $(LIBRARIES) ghostssh.c -o ghostssh

static: $(SOURCEFILES)
	read -s -p "SSH Password: " PASSWORD; \
	gcc $(CFLAGS) -DGHOSTSSH_USERNAME=\"$$USERNAME\" -DGHOSTSSH_PASSWORD=\"$$PASSWORD\" -I. $(LIBRARIES) ghostssh.c -o ghostssh
	chmod 100 ghostssh