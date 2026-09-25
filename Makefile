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

# ResourceLoader/PRI and Windows.Foundation.Uri (patches 0032, 0033), against
# the installed tree. WINGET_DIR=<unpacked winget> adds a makepri-written index.
test-resources:
	WINE=$(PREFIX)/bin/wine test/resources-gate.sh

# AppX/MSIX reading and signature trust (0034-0042); NETWORK=1 adds winget's
# real, Microsoft-signed source package.
test-appx:
	WINE=$(PREFIX)/bin/wine test/appx-gate.sh

# The real winget end to end (WINGET_DIR=<a user-supplied winget>; network).
test-winget:
	WINE=$(PREFIX)/bin/wine test/winget-e2e.sh

# The Compression API (0036).
test-compress:
	WINE=$(PREFIX)/bin/wine test/compress-gate.sh

# What Microsoft Edge needed (0049-0054); EDGE_MSI=<a user-supplied Edge
# enterprise MSI> also installs and runs Edge itself.
test-edge:
	WINE=$(PREFIX)/bin/wine test/edge-e2e.sh

# ClearType text and the Stained Glass scroll bars (0064-0066), by pixels.
test-theme:
	WINE=$(PREFIX)/bin/wine test/theme-gate.sh

# Cloaked windows, the primitive virtual desktops are built on (0067).
test-cloak:
	WINE=$(PREFIX)/bin/wine test/cloak-gate.sh

# Virtual desktops and Task View (0068).
test-vdesk:
	WINE=$(PREFIX)/bin/wine test/vdesk-gate.sh

# Wallpaper in any format, Windows' styles (0069).
test-wallpaper:
	WINE=$(PREFIX)/bin/wine test/wallpaper-gate.sh

# A standard user's shell folders (0070).
test-shellfolders:
	WINE=$(PREFIX)/bin/wine test/shellfolders-gate.sh

# The Windows key's shortcuts (0071).
test-shellkeys:
	WINE=$(PREFIX)/bin/wine test/shellkeys-gate.sh

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
