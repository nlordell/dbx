
CONTAINER = $(if $(findstring Darwin,$(shell uname -s)),container,podman)

CC      = cc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

SRCS  = src/cmd_create.c \
        src/err.c \
        src/engine_$(CONTAINER).c \
        src/fs.c \
        src/main.c \
        src/proc.c \
        src/ssh.c
OBJS  = $(patsubst %.c,%.o,$(SRCS))
IMAGE = ghcr.io/nlordell/dbx:latest

.PHONY: all
all: dbx ;

dbx: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c src/dbx.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: container
container: container/Containerfile container/init
	$(CONTAINER) build --tag $(IMAGE) container

.PHONY: clean
clean:
	rm -f dbx src/*.o
