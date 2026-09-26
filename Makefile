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

.PHONY: all build install test deb deps clean distclean lint test-rasdial test-taskdialog test-msitransform test-shellconsole test-pssig test-robocopy test-jscript test-darkmode test-taskbar test-explorer-dark

all: build

build:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./build.sh

install:
	PREFIX=$(PREFIX) JOBS=$(JOBS) DESTDIR=$(DESTDIR) DO_INSTALL=1 ./build.sh

# No user-visible "Windows" as our name (Microsoft's trademark): in the text
# our patches ADD, and in the English resources of the Wine programs and
# dialogs a user can reach (a series-applied tree, tools/lint-tree.sh).
# tools/trademark-allow.txt lists the exceptions, each with its reason.
# And every gate that runs Wine sources test/scratch-home.sh first, so no
# prefix of a gate links into the real HOME (a gate once emptied a real Desktop).
lint:
	@for f in $$(grep -l WINEPREFIX test/*.sh); do \
	    sed -n 2p "$$f" | grep -q '^\. "$$(dirname "$$0")/scratch-home.sh"$$' || \
	    { echo "$$f: line 2 must be: . \"\$$(dirname \"\$$0\")/scratch-home.sh\""; exit 1; }; done
	@T=$$(tools/lint-tree.sh) && \
	python3 tools/trademark-check.py --allow tools/trademark-allow.txt --patches patches/sg --root "$$T" \
	    --brand 'dlls/shell32/*' Wine --brand 'dlls/comdlg32/*' Wine --brand 'dlls/user32/*' Wine \
	    "$$T"/programs/*/*.rc "$$T"/dlls/shell32/*.rc "$$T"/dlls/comdlg32/*.rc "$$T"/dlls/user32/*.rc

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

# MSIX beyond one package (0200-0204): frameworks and the package graph,
# app execution aliases, updates, bundles' resource packages; WINGET_DIR=<a
# user-supplied winget> adds winget installing and removing an MSIX.
test-msix:
	WINE=$(PREFIX)/bin/wine test/msix-gate.sh

# A domain account's real SID (0205): tokens, files, HKCU, names, ACLs. Needs
# sudo and the second Unix user sgconf (a made-up domain record in /run).
test-domainsid:
	WINE=$(PREFIX)/bin/wine test/domainsid-gate.sh

# cmd's dir of a UNC path (0206). Needs sudo (a tmpfs share under /run).
test-uncdir:
	WINE=$(PREFIX)/bin/wine test/uncdir-gate.sh

# winex11: a program on another X display than the desktop's (0207), and the
# desktop's XRender drawing clipped by its windows (0208). Two Xvfb servers.
test-display:
	WINE=$(PREFIX)/bin/wine test/display-gate.sh

# The real winget end to end (WINGET_DIR=<a user-supplied winget>; network).
test-winget:
	WINE=$(PREFIX)/bin/wine test/winget-e2e.sh

# The Compression API (0036).
test-compress:
	WINE=$(PREFIX)/bin/wine test/compress-gate.sh

# WebView2 apps draw (0190-0191): WV2_INSTALLER=<the user's Evergreen runtime
# installer> WV2_SDK=<unpacked Microsoft.Web.WebView2 package>, or NETWORK=1.
test-webview2:
	WINE=$(PREFIX)/bin/wine test/webview2-gate.sh

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

# Dark mode: the Light style's Dark scheme, followed live by every program;
# dark title bars (DWMWA_USE_IMMERSIVE_DARK_MODE); the taskbar's Windows mode (0160-0163).
test-darkmode:
	WINE=$(PREFIX)/bin/wine test/darkmode-gate.sh

# A gate's prefix links its user folders into the gate's HOME, never the real one.
test-scratch-home:
	WINE=$(PREFIX)/bin/wine test/scratch-home-gate.sh

# The Run dialog: in front, typed into at once, our words and icon (0266).
test-rundialog:
	WINE=$(PREFIX)/bin/wine test/rundialog-gate.sh

# File Explorer follows the app mode, in Large icons and Details (0264, 0265).
test-explorer-dark:
	WINE=$(PREFIX)/bin/wine test/explorer-dark-gate.sh

# The taskbar honours Settings > Personalization > Taskbar (0164).
test-taskbar:
	WINE=$(PREFIX)/bin/wine test/taskbar-gate.sh

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

# A wineserver started on its own raises its open-files limit (0249).
test-serverfds:
	WINE=$(PREFIX)/bin/wine test/serverfds-gate.sh

# ExitWindowsEx shuts down and restarts the PC through sg-settingsctl (0241).
test-shutdown:
	WINE=$(PREFIX)/bin/wine test/shutdown-gate.sh

# A standard user's programs read the display configuration back (0242); a
# second Unix user as for test-pipeimp.
test-dispcfg:
	WINE=$(PREFIX)/bin/wine test/dispcfg-gate.sh

# A standard user's time zone and locale lookups (0243).
test-userlocale:
	WINE=$(PREFIX)/bin/wine test/userlocale-gate.sh

# A sandboxed program's own desktop comes up; nothing left spinning (0247).
test-sandboxdesk:
	WINE=$(PREFIX)/bin/wine test/sandboxdesk-gate.sh

# A requireAdministrator program: ERROR_ELEVATION_REQUIRED, ShellExecute asks
# the elevation broker (a stand-in here) (0259).
test-elevreq:
	WINE=$(PREFIX)/bin/wine test/elevreq-gate.sh

# The SCM checks who asks (0141); a second Unix user as for test-pipeimp.
test-scm-access:
	WINE=$(PREFIX)/bin/wine test/scm-access-gate.sh

# A service's own security descriptor, sc sdshow/sdset (0185); second Unix user as above.
test-scm-sd:
	WINE=$(PREFIX)/bin/wine test/scm-sd-gate.sh

# Audit events from the Linux side reach the Security log (0187); second Unix user as above.
test-audit:
	WINE=$(PREFIX)/bin/wine test/audit-gate.sh

# The administrative tools' Windows names: mmc/eventvwr/resmon/cleanmgr/msinfo32
# launchers, the .msc files and association, Start menu shortcuts, Win+X (0142, 0145);
# lusrmgr.msc/fsmgmt.msc and msinfo32 /report waiting for the file (0186).
test-admintools:
	WINE=$(PREFIX)/bin/wine test/admintools-gate.sh

# Magnifier and the On-Screen Keyboard: magnify.exe/osk.exe launchers, explorer's
# Win+Plus/Win+Minus/Win+Esc/Win+Ctrl+O (0181), appbars and the work area (0182).
test-a11y:
	WINE=$(PREFIX)/bin/wine test/a11y-gate.sh

# The X keyboard layout's keys: scan codes, AltGr, following a switch (0250-0251).
test-kbdlayout:
	WINE=$(PREFIX)/bin/wine test/kbdlayout-gate.sh

# The event log (0143, 0144): the service, the API, who may read Security;
# needs a second Unix user (SG_OTHER, default sgconf) and sudo -u to it.
test-eventlog:
	WINE=$(PREFIX)/bin/wine test/eventlog-gate.sh

# fontview.exe, the Fonts folder, per-user fonts (HKCU Fonts), shell: URLs (0183).
test-fonts:
	WINE=$(PREFIX)/bin/wine test/fonts-gate.sh

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

# VPN connections through RAS: rasapi32 and rasdial on sg-netctl (0400).
test-rasdial:
	WINE=$(PREFIX)/bin/wine test/rasdial-gate.sh

# A task dialog is made of comctl32 6's controls, manifest or not (0401).
test-taskdialog:
	WINE=$(PREFIX)/bin/wine test/taskdialog-gate.sh

# Transforms made here and deployed with msiexec TRANSFORMS= (0402).
test-msitransform:
	WINE=$(PREFIX)/bin/wine test/msitransform-gate.sh

# Console programs with no terminal get a console when they ask (0403).
test-shellconsole:
	WINE=$(PREFIX)/bin/wine test/shellconsole-gate.sh

# PowerShell script signatures; the certificate stores (0404, 0405).
test-pssig:
	WINE=$(PREFIX)/bin/wine test/pssig-gate.sh

# robocopy (0406).
test-robocopy:
	WINE=$(PREFIX)/bin/wine test/robocopy-gate.sh

# JScript GetObject; FileSystemObject with an empty path (0407).
test-jscript:
	WINE=$(PREFIX)/bin/wine test/jscript-gate.sh

# The shell's desktop icons (0090).
test-desktop-icons:
	WINE=$(PREFIX)/bin/wine test/desktop-icons-gate.sh

# The desktop: watched Desktop folders, selection, its menus, Alt+F4 asks (0261, 0262).
test-desktop:
	WINE=$(PREFIX)/bin/wine test/desktop-gate.sh

# File Explorer: layout, This PC, search, address bar, file operations (0110-0112).
test-explorer:
	WINE=$(PREFIX)/bin/wine test/explorer-gate.sh

# File Explorer round 2: thumbnails, icon sizes, tiles, groups, panes, Quick
# access, Send to, type names, drops on the navigation pane (0150-0157).
# SGZIP=<sg-shell's sg-zip64.exe> for Send to > Compressed (zipped) Folder.
test-explorer2:
	WINE=$(PREFIX)/bin/wine test/explorer2-gate.sh

# File Explorer round 3: video and PDF thumbnails, the thumbnail cache, picture
# and video details, Quick access remove and pin reordering (0244-0246).
# SG_PDF_HELPER=<sg-session's bin/sg-pdf> if it is not installed.
test-explorer3:
	WINE=$(PREFIX)/bin/wine test/explorer3-gate.sh

# A self-managed stack grows read-write; SD-less objects have an owner (0081, 0082).
test-stackgrow:
	WINE=$(PREFIX)/bin/wine test/stackgrow-gate.sh

test-objowner:
	WINE=$(PREFIX)/bin/wine test/objowner-gate.sh

# Notepad, our editor, for everything that runs notepad.exe (0100, 0101).
test-notepad:
	WINE=$(PREFIX)/bin/wine test/notepad-gate.sh

# Notepad's dark menu bar (0188) and Page Setup's header and footer (0100);
# needs cups-daemon (the gate runs a private cupsd for a test printer).
test-notepad-print:
	WINE=$(PREFIX)/bin/wine test/notepad-print-gate.sh

# Notepad's minimap (0100).
test-notepad-minimap:
	WINE=$(PREFIX)/bin/wine test/notepad-minimap-gate.sh

# A pipe end reports how much it can write (0083).
test-pipequota:
	WINE=$(PREFIX)/bin/wine test/pipequota-gate.sh

# What developer tools need (0320 on): the console code pages of a program
# whose output is redirected (dotnet CLI).
test-devtools:
	WINE=$(PREFIX)/bin/wine test/devtools-gate.sh

# What Telegram and Signal need of WinRT at start: ApiInformation's answers
# (0390), Windows.Storage.Streams' DataWriter and in-memory streams (0391).
test-winrtstreams:
	WINE=$(PREFIX)/bin/wine test/winrtstreams-gate.sh

# DwmFlush waits for the next vertical blank: Firefox's vsync (0170).
test-vsync:
	WINE=$(PREFIX)/bin/wine test/vsync-gate.sh

# The pinned Firefox shows a page with its GPU process and exits (0170; the
# compat suite's cached installer, network once).
test-firefox:
	WINE=$(PREFIX)/bin/wine test/firefox-e2e.sh

# Characters beyond the BMP (emoji) in GDI text (0171).
test-astral:
	WINE=$(PREFIX)/bin/wine test/astral-gate.sh

# The dynamic time zone conversions (0172).
test-tzex:
	WINE=$(PREFIX)/bin/wine test/tzex-gate.sh

# Windows.System.DispatcherQueue (0174).
test-dispatcherq:
	WINE=$(PREFIX)/bin/wine test/dispatcherq-gate.sh

# Windows 10 22H2; DXGIDeclareAdapterRemovalSupport (0175, 0176).
test-winver:
	WINE=$(PREFIX)/bin/wine test/winver-gate.sh

# DirectWrite finds the font GDI uses for a substituted name (0177).
test-dwlogfont:
	WINE=$(PREFIX)/bin/wine test/dwlogfont-gate.sh

# HKEY_CLASSES_ROOT is the merged view; the user's choices (0178, 0179).
# A shared prefix with a second Unix user (SG_OTHER, default sgconf) when there is one.
test-hkcr:
	WINE=$(PREFIX)/bin/wine test/hkcr-gate.sh

# The time zone and the clock through sg-admind (0221; SG_ADMIND= sg-shell's).
test-tzset:
	WINE=$(PREFIX)/bin/wine test/tzset-gate.sh

# DirectWrite hints a glyph at the size it is drawn at: cairo/GTK text (0222).
test-dwscale:
	WINE=$(PREFIX)/bin/wine test/dwscale-gate.sh

# Direct2D effects and contexts, DXGI's WARP adapter, shader reflection's
# feature level, Windows Animation, effect bounds, geometry combination and
# widening, drawing effects and command lists, as Paint.NET uses them
# (0223-0229).
test-d2dfx:
	WINE=$(PREFIX)/bin/wine test/d2dfx-gate.sh

# Windows.UI.Composition (compositor, visuals, brushes, drawing surfaces,
# desktop window targets), ID3D11Device5/ID3D11DeviceContext4/IDXGIDevice4,
# WIC half-float formats, window feedback settings (0229-0234; 0229's
# Direct2D drawing is in test-d2dfx).
test-wuicomp:
	WINE=$(PREFIX)/bin/wine test/wuicomp-gate.sh

# What popular installers need: a COM service by a long name with its
# parameters, key DACLs through write handles, [string] typelib marshalling,
# internet shortcuts, process group affinity (0235-0239).
test-appfix:
	WINE=$(PREFIX)/bin/wine test/appfix-gate.sh

# What Discord and Spotify need at start: the SSL policy's ignore-unknown-
# revocation flags, ProcessHandleTable/ProcessHandleCount (64- and 32-bit),
# the shell's tray settings key, DecryptMessage's missing count (0380-0383).
test-commapps:
	WINE=$(PREFIX)/bin/wine test/commapps-gate.sh

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

# Office and documents compat round (0300-0304).
test-dcrt-print:
	WINE=$(PREFIX)/bin/wine test/dcrt-print-gate.sh
test-powershell:
	WINE=$(PREFIX)/bin/wine test/powershell-gate.sh
test-cryptbase:
	WINE=$(PREFIX)/bin/wine test/cryptbase-gate.sh
test-msica:
	WINE=$(PREFIX)/bin/wine test/msica-gate.sh
test-office-docs:
	WINE=$(PREFIX)/bin/wine test/office-docs-gate.sh
