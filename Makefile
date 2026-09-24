# wine-sg -- Wine built for Stained Glass OS.
#
#   make build     build Wine (about 12 minutes on 12 cores)
#   make install   install to $(PREFIX), default /opt/wine-sg
#   make test      the gate: prove 32-bit Windows code runs with no i386 Linux
#   make deb       build the .deb
#
# The reason this repo exists is one configure flag: --enable-archs=i386,x86_64.
# See ADR 0002 and ADR 0005 in the stained-glass repo.

PREFIX  ?= /opt/wine-sg
DESTDIR ?=
JOBS    ?= $(shell nproc)

.PHONY: all build install test deb deps clean distclean

all: build

build:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./build.sh

install:
	PREFIX=$(PREFIX) JOBS=$(JOBS) DESTDIR=$(DESTDIR) DO_INSTALL=1 ./build.sh

# Runs against the *installed* tree, not the build tree, because what we ship
# is what we care about. Install first.
test:
	PREFIX=$(PREFIX) test/wow64-gate.sh

deb:
	dpkg-buildpackage -us -uc -b

deps:
	sudo apt-get build-dep -y wine
	sudo apt-get install -y gcc-mingw-w64-i686 gcc-mingw-w64-x86-64 \
	                        mingw-w64-tools libsane-dev \
	                        librsvg2-bin icoutils imagemagick

clean:
	rm -rf build/obj

distclean:
	rm -rf build
