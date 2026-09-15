.POSIX:

PREFIX  = $(HOME)/.local
INSTALL = install

ifeq ($(OS),Darwin)
	CONTAINER = container
else
	CONTAINER = podman
endif

IMAGE = ghcr.io/nlordell/dbx:latest

.PHONY: all
all:
	@echo "usage: make [install|container]"

.PHONY: install
install: dbx
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 755 dbx $(PREFIX)/bin/dbx

.PHONY: container
container: container/Containerfile container/init
	$(CONTAINER) build --tag $(IMAGE) container

.PHONY: check
check: dbx container/init container/stop
	shellcheck $^
	shfmt -d $^
