.PHONY: build clean doc
build:
	dune build

clean:
	dune clean

doc:
	dune build @doc

VERSION:=0.12
NAME=ocaml-qfs-$(VERSION)

.PHONY: release
release:
	git tag -a -m $(VERSION) $(VERSION)
	git archive --prefix=$(NAME)/ $(VERSION) | gzip > $(NAME).tar.gz
