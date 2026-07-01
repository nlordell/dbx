DBX_IMAGE ?= dbx

.PHONY: all
all: .container ssh/id_ed25519 ;

.PHONY: install
install: all
	mkdir -p $(HOME)/.local/bin
	ln -s $(PWD)/dbx $(HOME)/.local/bin/dbx

.container: Containerfile dbx-init ssh/id_ed25519.pub
	container build --pull --tag $(DBX_IMAGE) .
	touch $@

ssh/id_% ssh/id_%.pub:
	ssh-keygen -C "$$(id -un)@$$(hostname)" -f ssh/id_$* -N "" -t $*
