.PHONY: all
all: ssh/cofig .container;
	@ echo "install with 'make install'"

.PHONY: install
install: all
	mkdir -p $(HOME)/.local/bin
	ln -s $(PWD)/dbx $(HOME)/.local/bin/dbx

ssh/config: ssh/config.in ssh/id_ed25519.pub
	cat $< \
		| sed -e 's|$$PWD|$(PWD)|' \
		> $@

.container: ssh/id_ed25519.pub

ssh/id_% ssh/id_%.pub:
	ssh-keygen -C "$$(id -un)@$$(hostname)" -f ssh/id_$* -N "" -t $*
