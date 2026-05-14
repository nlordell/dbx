.PHONY: all
all: ;
	@ echo "install with 'make install'"

.PHONY: install
install: all
	mkdir -p $(HOME)/.local/bin
	ln -s $(PWD)/dbx $(HOME)/.local/bin/dbx

ssh/id_% ssh/id_%.pub:
	ssh-keygen -C "$$(id -un)@$$(hostname)" -f ssh/id_$* -N "" -t $*
