CC      = cc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

SRCS = $(shell find src -name '*.c')
OBJS = $(patsubst %.c,%.o,$(SRCS))

CONTAINER = $(if $(findstring Darwin,$(shell uname -s)),container,podman)
IMAGE     = dbx-next

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
