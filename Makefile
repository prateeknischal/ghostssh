override CFLAGS += -Wall -g
SOURCEFILES = $(wildcard *.c *.h)
SOURCEFILES_WITH_CONFIG = $(wildcard *.c *.h *.py)

build: $(SOURCEFILES)
	gcc $(CFLAGS) -I. -lssh2 -lpthread ghostssh.c -o ghostssh

static: $(SOURCEFILES_WITH_CONFIG)
	python configure.py
	gcc $(CFLAGS) -DGHOSTSSH_STATIC -I. -lssh2 -lpthread ghostssh.c -o ghostssh
	chmod 100 ghostssh
