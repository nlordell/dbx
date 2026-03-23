GO := docker run -it --rm --security-opt label=disable -v $(PWD):/dbx -w /dbx docker.io/library/golang go

.PHONY: all
all: dbx-proxy ;

.PHONY: install
install: all
	mkdir -p $(HOME)/.local/bin
	ln -s $(PWD)/dbx $(HOME)/.local/bin/dbx

dbx-proxy: dbx-proxy.go
	$(GO) build -o $@ $<

.PHONY: fmt
fmt:
	$(GO) fmt dbx-proxy.go

.PHONY: clean
clean:
	rm -f dbx-proxy
