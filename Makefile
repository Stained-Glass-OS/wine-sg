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

# The user's regional format is their choice (0168).
test-region:
	WINE=$(PREFIX)/bin/wine test/region-gate.sh

# A standard user's shell folders (0070).
test-shellfolders:
	WINE=$(PREFIX)/bin/wine test/shellfolders-gate.sh

# The Windows key's shortcuts (0071).
test-shellkeys:
	WINE=$(PREFIX)/bin/wine test/shellkeys-gate.sh

# control.exe follows App Paths (0072).
test-control:
	WINE=$(PREFIX)/bin/wine test/control-gate.sh

# calc/mspaint/snippingtool launchers, taskmgr/wmplayer hand-off, Win+Shift+S, Ctrl+Shift+Esc (0120-0123).
test-handoff:
	WINE=$(PREFIX)/bin/wine test/handoff-gate.sh

# A named pipe server impersonates its client, not itself (0140); needs a
# second Unix user (SG_OTHER, default sgconf) and passwordless sudo -u to it.
test-pipeimp:
	WINE=$(PREFIX)/bin/wine test/pipeimp-gate.sh

# The SCM checks who asks (0141); a second Unix user as for test-pipeimp.
test-scm-access:
	WINE=$(PREFIX)/bin/wine test/scm-access-gate.sh

# The administrative tools' Windows names: mmc/eventvwr/resmon/cleanmgr/msinfo32
# launchers, the .msc files and association, Start menu shortcuts, Win+X (0142, 0145).
test-admintools:
	WINE=$(PREFIX)/bin/wine test/admintools-gate.sh

# The event log (0143, 0144): the service, the API, who may read Security;
# needs a second Unix user (SG_OTHER, default sgconf) and sudo -u to it.
test-eventlog:
	WINE=$(PREFIX)/bin/wine test/eventlog-gate.sh

# Startup items disabled in Task Manager (0125).
test-startup:
	WINE=$(PREFIX)/bin/wine test/startup-gate.sh

# PrintWindow of another process's window (0076).
test-printwindow:
	WINE=$(PREFIX)/bin/wine test/printwindow-gate.sh

# WinSta0\Default is the user's desktop (0080).
test-default-desktop:
	WINE=$(PREFIX)/bin/wine test/default-desktop-gate.sh

# Real applications, installed and run (network, ~1 h; not in `make test`).
test-compat:
	SG_DEFAULTS=$${SG_DEFAULTS:-../sg-shell/theme} WINE=$(PREFIX)/bin/wine test/compat/run.sh

# Native stdio, ipconfig and netsh on sg-netctl (0078-0079).
test-netsh:
	WINE=$(PREFIX)/bin/wine test/netsh-gate.sh

# The shell's desktop icons (0090).
test-desktop-icons:
	WINE=$(PREFIX)/bin/wine test/desktop-icons-gate.sh

# File Explorer: layout, This PC, search, address bar, file operations (0110-0112).
test-explorer:
	WINE=$(PREFIX)/bin/wine test/explorer-gate.sh

# A self-managed stack grows read-write; SD-less objects have an owner (0081, 0082).
test-stackgrow:
	WINE=$(PREFIX)/bin/wine test/stackgrow-gate.sh

test-objowner:
	WINE=$(PREFIX)/bin/wine test/objowner-gate.sh

# Notepad, our editor, for everything that runs notepad.exe (0100, 0101).
test-notepad:
	WINE=$(PREFIX)/bin/wine test/notepad-gate.sh

# A pipe end reports how much it can write (0083).
test-pipequota:
	WINE=$(PREFIX)/bin/wine test/pipequota-gate.sh

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
