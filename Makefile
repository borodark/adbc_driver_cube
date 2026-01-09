SRC ?= /home/io/projects/learn_erl/adbc
TRIPLET ?= x86_64-linux-gnu
VERSION ?= $(shell git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')

.PHONY: sync build package

sync:
	./scripts/sync_from_fork.sh $(SRC)

build:
	./scripts/build.sh

package:
	VERSION=$(VERSION) TRIPLET=$(TRIPLET) ./scripts/package.sh
