.DEFAULT_GOAL := build
TRIPLET ?= x86_64-linux-gnu
VERSION ?= $(shell git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')

.PHONY: build package clean test test_live

build:
	./scripts/build.sh

package:
	VERSION=$(VERSION) TRIPLET=$(TRIPLET) ./scripts/package.sh

clean:
	rm -rf build dist

test:
	./tests/cpp/compile.sh

test_live: test
	./tests/cpp/run.sh
