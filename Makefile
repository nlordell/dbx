.PHONY: all
all: .container ssh/config ;

.PHONY: install
install: all
	mkdir -p $(HOME)/.local/bin
	ln -s $(PWD)/dbx $(HOME)/.local/bin/dbx

.container: Containerfile dbx-init ssh/id_ed25519.pub
	container build --build-arg USER=$(USER) --pull --tag dbx .
	touch $@

ssh/config: ssh/config.in ssh/id_ed25519.pub
	cat $< \
		| sed -e 's|$$PWD|$(PWD)|' \
		> $@

ssh/id_% ssh/id_%.pub:
	ssh-keygen -C "$$(id -un)@$$(hostname)" -f ssh/id_$* -N "" -t $*
