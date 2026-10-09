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

.PHONY: all build install test deb deps clean distclean lint test-rasdial test-taskdialog test-msitransform test-shellconsole test-pssig test-robocopy test-jscript test-darkmode test-taskbar test-explorer-dark test-d2dlayer test-d2dsvg test-kbchar test-d2dgraph test-advcolor test-focusactivate test-drvkey test-mmdevkey test-ofn test-selflink test-unixexec test-taskview-linux test-xproc-vk test-firstpaint test-fileopadmin test-msiallusers test-verblabel test-taskbarlooks test-fileopresponsive test-mapdrive test-openwith test-drag test-deskhover test-deskerase test-dpifollow test-hungwake test-savename test-folderspeed test-shortcut test-topbar test-msiseltree test-statuscount test-folderwatch test-dcompalpha test-spooler test-desktopbg test-layeredblend test-linuxicon test-shellnew test-glassvk test-elevfolders test-displaycfg test-pss test-pyimports test-ownerrights test-ptracecap test-proxyblanket test-comlauncher test-cloakpaint test-gpupriority test-dbghelpinline test-regfsync test-sysversions test-drivewatch test-systemtemp test-desktopfront test-mcastif test-prtsc test-stiinf test-usbtree test-gpahook test-autoplay test-envreload test-iconhandler test-gdipfamily test-dirshare test-elevtray test-anotherwindow test-threadclass test-fetabs test-linuxbash test-embedmaxgrab test-toastactivate test-netfx3 test-smbshare test-smbview test-netuse test-mapcred test-wmiprinter test-cupsmodel test-devkeyread test-cupsrefresh test-pentouch test-drvpkg test-psdriver test-unidrvxl test-unidrvrast test-psplugin test-cpsui test-drvui test-netport test-printproc test-dwsubst test-ptcaps test-smoothshapes test-iconframes test-touch test-keybutton test-hiddenmax test-tlbbase test-comsvc test-privacysettings test-dwspacing test-msgboxicons test-credkeyring test-latepen test-wininet-listentimeout test-onlineid-ticket test-richtxsrv2 test-d2dsharedview test-rotransformerror test-cputime test-fileinfo test-procsettings test-memprotect test-userapis test-kbmisc test-cpusets test-werreg test-devfamily test-aclentries test-richinit test-bufpaint test-dwmattrs test-muipath test-stockicon test-enumjobs test-replacefile test-svcdepend test-sysshutdown test-advsec test-shcorebits test-fontquery test-guithread test-ipnotify test-apprestart test-minidumpx test-joblimits test-fmtsupport test-printnotify test-diskinfo test-perfcount test-taskdisp test-taskinfo test-taskxml test-bitsjob test-bitsmore test-mmcss test-devtree test-pinterfaceiid test-taskprogress test-winappid test-childpolicy test-smallstubs2 test-asynclock test-uilangs test-shellapp test-shelldisp test-wshshell test-textstreams test-fsoitems test-wshnet test-vbsbuiltins test-vbsdyncode test-jsstubs test-nsplookup test-wscatalog test-gdipquality test-hotkeyexec test-zipfolder test-thumbbar test-xplacement test-tsfsinks test-certresync test-inputpane test-jumplist test-evtquery test-audiotopo test-dxcore test-dmanip test-imglist2 test-lvtv test-toolbar2 test-ccmisc test-ole32misc test-oapict test-tlbcreate test-roerror test-guires test-powerreq test-ncryptksp test-cryptrest test-etwtrace test-uiaevents test-msgfilter test-uiabridge test-shellmisc test-oleaccstd test-sockmisc test-netlist

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

# The taskbar's desktop pager, four desktops, Task View reopens (0446).
test-vdpager:
	WINE=$(PREFIX)/bin/wine test/vdpager-gate.sh

# A standard user trusts the machine's root certificates (0447).
test-userroots:
	WINE=$(PREFIX)/bin/wine test/userroots-gate.sh

# A window focused before it is shown gets the keyboard (0449).
test-focusmap:
	WINE=$(PREFIX)/bin/wine test/focusmap-gate.sh

# A folder's right-click menu: New > Folder, Shortcut, Text Document (0450).
test-newtext:
	WINE=$(PREFIX)/bin/wine test/newtext-gate.sh

# Push buttons and message boxes as Windows 10 draws them (0454).
test-flatbuttons:
	WINE=$(PREFIX)/bin/wine test/flatbuttons-gate.sh

# A grant to BUILTIN\\Users reaches the machine's users: the prefix's group (0456).
test-usersacl:
	WINE=$(PREFIX)/bin/wine test/usersacl-gate.sh

# A key granted to BUILTIN\\Users is theirs to write from a 64-bit program's
# view of the 32-bit registry: the keys on the way are passed through (0459).
test-regwalk:
	WINE=$(PREFIX)/bin/wine test/regwalk-gate.sh

# A program's crash is told in this system's words, not Wine's (0460).
test-crashbrand:
	WINE=$(PREFIX)/bin/wine test/crashbrand-gate.sh

# Another thread's hidden window leaves the foreground where it is: Firefox
# stays full screen (0461).
test-fgthread:
	WINE=$(PREFIX)/bin/wine test/fgthread-gate.sh

# The interface fonts' names are families ("Segoe UI"), not faces' full names:
# Firefox's system-ui font (0462).
test-metricsfont:
	WINE=$(PREFIX)/bin/wine test/metricsfont-gate.sh

# The administrative tools' launchers carry icons; .msc files show mmc.exe's (0463).
test-admintoolicons:
	WINE=$(PREFIX)/bin/wine test/admintoolicons-gate.sh

# Right-clicking the taskbar opens its menu, Start the quick link menu (0464).
test-traymenu:
	WINE=$(PREFIX)/bin/wine test/traymenu-gate.sh

# On a shared prefix SYSTEM's token is elevated; a standard user's is not (0465).
test-sysrelevated:
	WINE=$(PREFIX)/bin/wine test/sysrelevated-gate.sh

# SYSTEM may change the permissions of a file another account owns: CAP_FOWNER,
# permitted only, effective for one fchmod (0466).
test-sysfowner:
	WINE=$(PREFIX)/bin/wine test/sysfowner-gate.sh

# Every user of a shared prefix has APPDATA, LOCALAPPDATA, HOMEDRIVE, HOMEPATH (0467).
test-logonvars:
	WINE=$(PREFIX)/bin/wine test/logonvars-gate.sh

# A drive's menu: Open first, Eject for discs and removable drives, no Cut/Delete (0468).
test-drivemenu:
	WINE=$(PREFIX)/bin/wine test/drivemenu-gate.sh

# The Network folder: computers, their shares, \\\\server paths (0469).
test-netplaces:
	WINE=$(PREFIX)/bin/wine test/netplaces-gate.sh

# An environment block's USERPROFILE (ProfileImagePath) and no literal %variable% (0470).
test-envblock:
	WINE=$(PREFIX)/bin/wine test/envblock-gate.sh

# A symlink opened as itself says REPARSE_POINT (0471): Office's staging.
test-reparseinfo:
	WINE=$(PREFIX)/bin/wine test/reparseinfo-gate.sh

# ShellExecute of a long URL (a mailto: with a body) reaches its handler (0472).
test-longurl:
	WINE=$(PREFIX)/bin/wine test/longurl-gate.sh

# A program's taskbar button menu: its name, Report a problem, Close (0473).
test-btnmenu:
	WINE=$(PREFIX)/bin/wine test/btnmenu-gate.sh

# Shares with a name and password: own connection first, net use /user (0474).
test-netlogon:
	WINE=$(PREFIX)/bin/wine test/netlogon-gate.sh

# msdelta refuses instead of killing Click-to-Run (0475).
test-msdelta:
	WINE=$(PREFIX)/bin/wine test/msdelta-gate.sh

# A window menu's Move and Size follow the pointer until a click (0476).
test-movesize:
	WINE=$(PREFIX)/bin/wine test/movesize-gate.sh

# Closing a desktop's last window stays on that desktop (0477).
test-vdstay:
	WINE=$(PREFIX)/bin/wine test/vdstay-gate.sh

# Our own folder, file, text file and Notepad icons (theme/icons.py, 0478).
test-icons:
	WINE=$(PREFIX)/bin/wine test/icons-gate.sh

# Save As > Desktop shows the desktop's files, not namespace links (0480).
test-savedesk:
	WINE=$(PREFIX)/bin/wine test/savedesk-gate.sh

# After a resolution change the taskbar is on the new screen, icons by the clock (0481).
test-dispchange:
	WINE=$(PREFIX)/bin/wine test/dispchange-gate.sh

# The Rounded style rounds windows' corners (0483).
test-rounded:
	WINE=$(PREFIX)/bin/wine test/rounded-gate.sh

# The Rounded style's edit boxes, list boxes and views are round too; radio buttons whole (0740).
test-roundedctl:
	WINE=$(PREFIX)/bin/wine test/roundedctl-gate.sh

# Programs pinned to the taskbar (0485).
test-taskpins:
	WINE=$(PREFIX)/bin/wine test/taskpins-gate.sh

# The Horizon and Glass looks' window frames (0742).
test-eraframes:
	WINE=$(PREFIX)/bin/wine test/eraframes-gate.sh

# The Horizon and Glass looks' controls: their colour schemes (0743, theme/eras.py).
test-eraschemes:
	WINE=$(PREFIX)/bin/wine test/eraschemes-gate.sh

# What the desktop's compositor (sg-deskcomp) is told: its picture, window shadows (0744).
test-deskprops:
	WINE=$(PREFIX)/bin/wine test/deskprops-gate.sh

# A new user's first taskbar pins, from HKLM DefaultPins (0741).
test-defaultpins:
	WINE=$(PREFIX)/bin/wine test/defaultpins-gate.sh

# 0602: pinned programs show their own icons (File Explorer: the folder).
test-pinicon:
	WINE=$(PREFIX)/bin/wine test/pinicon-gate.sh

# 0609: the desktop keeps winspool loaded (printers load once a session).
test-spooler:
	WINE=$(PREFIX)/bin/wine test/spooler-gate.sh

# 0610: where a window leaves the desktop, X shows the desktop at once.
test-desktopbg:
	WINE=$(PREFIX)/bin/wine test/desktopbg-gate.sh

# 0611: an alpha-layered popup is blended over what is under it.
test-layeredblend:
	WINE=$(PREFIX)/bin/wine test/layeredblend-gate.sh

# 0612: Linux windows' taskbar buttons show their apps' icons.
test-linuxicon:
	WINE=$(PREFIX)/bin/wine test/linuxicon-gate.sh

# 0613: New offers the file types programs register (ShellNew).
test-shellnew:
	WINE=$(PREFIX)/bin/wine test/shellnew-gate.sh

# 0615: another process's Vulkan frames in a glass popup are blended (Chrome's bubbles).
test-glassvk:
	WINE=$(PREFIX)/bin/wine test/glassvk-gate.sh

# A Vulkan glass popup keeps its see-through edge under the compositor (0753).
test-glasspixel:
	test/glasspixel-gate.sh

# The programs that start at sign-in: Run keys and Startup folders (0754).
test-signin-startup:
	WINE=$(PREFIX)/bin/wine test/signin-startup-gate.sh

# A shell started again shows the desktop in its own X window (0755).
test-shellrestart:
	WINE=$(PREFIX)/bin/wine test/shellrestart-gate.sh

# An elevated program's tray icon is in the shell's tray, and its clicks
# reach it through UIPI (0771).
test-elevtray:
	WINE=$(PREFIX)/bin/wine test/elevtray-gate.sh

# Shift+click or a middle click on a taskbar button opens another window (0773).
test-anotherwindow:
	WINE=$(PREFIX)/bin/wine test/anotherwindow-gate.sh

# A program's second thread registers window classes (0775).
test-threadclass:
	WINE=$(PREFIX)/bin/wine test/threadclass-gate.sh

# File Explorer's tabs (0774).
test-fetabs:
	WINE=$(PREFIX)/bin/wine test/fetabs-gate.sh

# bash.exe: the Linux side's bash from PowerShell or cmd (0776).
test-linuxbash:
	WINE=$(PREFIX)/bin/wine test/linuxbash-gate.sh

# A batch file is read in the console's code page (0756).
test-batchcp:
	WINE=$(PREFIX)/bin/wine test/batchcp-gate.sh

# sysdm.cpl: System Properties opens the Control Panel's own (0757).
test-sysdm:
	WINE=$(PREFIX)/bin/wine test/sysdm-gate.sh

# A Linux window's stand-in answers close and restore (0759).
test-linuxstandin:
	WINE=$(PREFIX)/bin/wine test/linuxstandin-gate.sh

# 0617: an elevated program's Start menu and desktop are all users'.
test-elevfolders:
	WINE=$(PREFIX)/bin/wine test/elevfolders-gate.sh

# 0619: the display devices a session records stay writable by its user.
test-displaycfg:
	WINE=$(PREFIX)/bin/wine test/displaycfg-gate.sh

# 0620: kernel32's process snapshot API (BleachBit's packed python312.dll).
test-pss:
	WINE=$(PREFIX)/bin/wine test/pss-gate.sh

# 0621: what pywin32 imports, exported and working (BleachBit's pythoncom).
test-pyimports:
	WINE=$(PREFIX)/bin/wine test/pyimports-gate.sh

# 0622: OWNER RIGHTS ACEs are the owner's (CPython's os.mkdir(path, 0o700)).
test-ownerrights:
	WINE=$(PREFIX)/bin/wine test/ownerrights-gate.sh

# 0624: the SYSTEM account's own processes may be traced (CAP_SYS_PTRACE when needed).
test-ptracecap:
	WINE=$(PREFIX)/bin/wine test/ptracecap-gate.sh

# 0627: a COM proxy keeps its security blanket (Omaha: Brave's installer).
test-proxyblanket:
	WINE=$(PREFIX)/bin/wine test/proxyblanket-gate.sh

# 0628: a COM local server's launcher may exit first (Omaha's ...OnDemand.exe).
test-comlauncher:
	WINE=$(PREFIX)/bin/wine test/comlauncher-gate.sh

# 0629: a cloaked window is still painted (Brave uncloaks in WM_NCPAINT).
test-cloakpaint:
	WINE=$(PREFIX)/bin/wine test/cloakpaint-gate.sh

# 0633: gdi32 exports the GPU scheduling priority class calls (OBS Studio).
test-gpupriority:
	WINE=$(PREFIX)/bin/wine test/gpupriority-gate.sh

# 0634: dbghelp skips unresolvable inline sites in step (OBS's PDBs).
test-dbghelpinline:
	WINE=$(PREFIX)/bin/wine test/dbghelpinline-gate.sh

# 0635: registry hives are fsync'd before they replace the old ones.
test-regfsync:
	WINE=$(PREFIX)/bin/wine test/regfsync-gate.sh

# 0636: shell32 and vcruntime140 versions installers look for (TortoiseGit's MSI).
test-sysversions:
	WINE=$(PREFIX)/bin/wine test/sysversions-gate.sh

# 0637: drives and discs coming and going reach File Explorer and programs.
test-drivewatch:
	WINE=$(PREFIX)/bin/wine test/drivewatch-gate.sh

# 0638: GetTempPath2 gives SYSTEM %SystemRoot%\SystemTemp\.
test-systemtemp:
	WINE=$(PREFIX)/bin/wine test/systemtemp-gate.sh

# IP_MULTICAST_IF by interface index (0639): DYMO Connect's web service.
test-mcastif:
	WINE=$(PREFIX)/bin/wine test/mcastif-gate.sh

# Print Screen goes to a screenshot tool that registers it (0640): Greenshot.
test-prtsc:
	WINE=$(PREFIX)/bin/wine test/prtsc-gate.sh

# A scanner's INF: Include/Needs and the still image class's lines (0641,
# sti.inf from 0642): the Ambir ImageScan Pro 490i.
test-stiinf:
	WINE=$(PREFIX)/bin/wine test/stiinf-gate.sh

# The USB tree scanner software walks (0643), usbscan.sys installed (0642).
test-usbtree:
	WINE=$(PREFIX)/bin/wine test/usbtree-gate.sh

# kernel32's GetProcAddress goes through kernelbase (0644): TWAINDSM's hook.
test-gpahook:
	WINE=$(PREFIX)/bin/wine test/gpahook-gate.sh

# A USB drive that arrives gets a notification that opens it (0748).
test-autoplay:
	WINE=$(PREFIX)/bin/wine test/autoplay-gate.sh

# A changed environment reaches the shell, and new programs (0749).
test-envreload:
	WINE=$(PREFIX)/bin/wine test/envreload-gate.sh

# The taskbar's Horizon and Glass looks, beside the flat default (0600).
test-taskbarlooks:
	WINE=$(PREFIX)/bin/wine test/taskbarlooks-gate.sh

# Our round and slanted shapes drawn smooth (1240): taskbar plates, tabs, chevrons.
test-smoothshapes:
	WINE=$(PREFIX)/bin/wine test/smoothshapes-gate.sh

# Our icons have every frame, 16-256 px (1241, theme/icons.py), in the sources and installed.
test-iconframes:
	WINE=$(PREFIX)/bin/wine test/iconframes-gate.sh

# XML Schema patterns with MSXML's \uXXXX escapes (0489): Office's installer.
test-xsdpattern:
	WINE=$(PREFIX)/bin/wine test/xsdpattern-gate.sh

# removeChild of a node replaceChild took out (0490): Office's installer.
test-msxml-replace:
	WINE=$(PREFIX)/bin/wine test/msxml-replace-gate.sh

# The Software Licensing client's store (0491): Office's installer.
test-sppc:
	WINE=$(PREFIX)/bin/wine test/sppc-gate.sh

# A process's activation filter (0493): Word.
test-actfilter:
	WINE=$(PREFIX)/bin/wine test/actfilter-gate.sh

# SetFileShortName (0494): a helper Word starts.
test-shortname:
	WINE=$(PREFIX)/bin/wine test/shortname-gate.sh

# The session's Linux programs' windows on the taskbar (0496).
test-xwin-taskbar:
	WINE=$(PREFIX)/bin/wine test/xwin-taskbar-gate.sh

# AVL tables, RtlIsNameInExpression, FindNextFileNameW (0498, 0499): Office.
test-avl:
	WINE=$(PREFIX)/bin/wine test/avl-gate.sh

# Elevated windows on the taskbar, minimizable (0500).
test-elevmin:
	WINE=$(PREFIX)/bin/wine test/elevmin-gate.sh

# DirectWrite: a collection's font set (0503): Office's text.
test-dwfontset:
	WINE=$(PREFIX)/bin/wine test/dwfontset-gate.sh

# The C++ library's time zone database (0504): Word.
test-tzdb:
	WINE=$(PREFIX)/bin/wine test/tzdb-gate.sh

# msvcp's _Sinh/_Cosh (0509): std::complex's sin/cos/sinh/cosh/tan -- SG
# Office's (LibreOffice for Windows) IMSIN, IMSINH ...
test-sinh:
	WINE=$(PREFIX)/bin/wine test/sinh-gate.sh

# RegRestoreKey restores binary and text hives (0506): Office's virtual registry.
test-regrestore:
	WINE=$(PREFIX)/bin/wine test/regrestore-gate.sh

# chakra.dll, the JavaScript hosting API on QuickJS (0508): Office's React Native panes.
test-chakra:
	WINE=$(PREFIX)/bin/wine test/chakra-gate.sh

# RtlValidRelativeSecurityDescriptor (0507): offreg opens Office's virtual registry hives.
test-relsd:
	WINE=$(PREFIX)/bin/wine test/relsd-gate.sh

# vcruntime140's __C_specific_handler_noexcept (0510): Word's sign-in click.
test-noexcept:
	WINE=$(PREFIX)/bin/wine test/noexcept-gate.sh

# CalculatePopupWindowPosition (0511): Word's "Sign in or create account".
test-popuppos:
	WINE=$(PREFIX)/bin/wine test/popuppos-gate.sh

# Right-click Delete goes to the Recycle Bin without a hidden prompt (0514).
test-recycle:
	WINE=$(PREFIX)/bin/wine test/recycle-gate.sh

# File Explorer's Sort, View and Refresh glyphs (0515), from a screenshot.
test-explorer-glyphs:
	WINE=$(PREFIX)/bin/wine test/explorer-glyphs-gate.sh

# Windows do not flash black when shown (0519).
test-blackflash:
	WINE=$(PREFIX)/bin/wine test/blackflash-gate.sh

# A window activated from another process (the taskbar) gets the keyboard (0527).
test-focusactivate:
	WINE=$(PREFIX)/bin/wine test/focus-activate-gate.sh

# Win32_Printer lists the printers (0870).
test-wmiprinter:
	WINE=$(PREFIX)/bin/wine test/wmiprinter-gate.sh

# Printer makers' Windows drivers (1020-1022): packages install as they
# ship, makers' own drivers print through their blits and WritePrinter;
# a PPD on our PSCRIPT5 (wineps) sends every option; HP-style PCL XL
# vector plug-ins on Unidrv.
test-drvpkg:
	WINE=$(PREFIX)/bin/wine test/drvpkg-gate.sh
test-psdriver:
	WINE=$(PREFIX)/bin/wine test/psdriver-gate.sh
test-unidrvxl:
	WINE=$(PREFIX)/bin/wine test/unidrvxl-gate.sh
test-unidrvrast:
	WINE=$(PREFIX)/bin/wine test/unidrvrast-gate.sh
test-psplugin:
	WINE=$(PREFIX)/bin/wine test/psplugin-gate.sh
test-cpsui:
	WINE=$(PREFIX)/bin/wine test/cpsui-gate.sh
test-drvui:
	WINE=$(PREFIX)/bin/wine test/drvui-gate.sh
test-netport:
	WINE=$(PREFIX)/bin/wine test/netport-gate.sh
test-printproc:
	WINE=$(PREFIX)/bin/wine test/printproc-gate.sh

# A CUPS printer's driver is named after its make and model (0871).
test-cupsmodel:
	WINE=$(PREFIX)/bin/wine test/cupsmodel-gate.sh

# A printer added to CUPS during a session reaches Windows programs (0873).
test-cupsrefresh:
	WINE=$(PREFIX)/bin/wine test/cupsrefresh-gate.sh

# A standard user reads a device's own key (0872).
test-devkeyread:
	WINE=$(PREFIX)/bin/wine test/devkeyread-gate.sh

# A standard user reads a device's driver key (0580).
test-drvkey:
	WINE=$(PREFIX)/bin/wine test/drvkey-gate.sh

# A standard user's audio endpoints have properties (0581).
test-mmdevkey:
	WINE=$(PREFIX)/bin/wine test/mmdevkey-gate.sh

# The file dialogs as Windows shows them (0582).
test-ofn:
	WINE=$(PREFIX)/bin/wine test/ofn-gate.sh

# A shell folder is never linked to itself (0583).
test-selflink:
	WINE=$(PREFIX)/bin/wine test/selflink-gate.sh

# Starting a Unix program leaks nothing (0584).
test-unixexec:
	WINE=$(PREFIX)/bin/wine test/unixexec-gate.sh

# Machine components found by an account that may only read the Installer
# keys (0701): Word's "(6)" after Office's integrator ran.
test-msireadonly:
	WINE=$(PREFIX)/bin/wine test/msireadonly-gate.sh

# Wine's windows come in front of Linux programs' windows (0700).
test-desktopfront:
	WINE=$(PREFIX)/bin/wine test/desktopfront-gate.sh

# The Linux programs' windows in Task View (0585).
test-taskview-linux:
	WINE=$(PREFIX)/bin/wine test/taskview-linux-gate.sh

# Another process renders into a window with Vulkan (0586).
test-xproc-vk:
	WINE=$(PREFIX)/bin/wine test/xproc-vk-gate.sh

# A window is not shown black before its program paints it (0587).
test-firstpaint:
	WINE=$(PREFIX)/bin/wine test/firstpaint-gate.sh

# A folder the user may not write to asks for an administrator (0589).
test-fileopadmin:
	WINE=$(PREFIX)/bin/wine test/fileopadmin-gate.sh

# A package for all users is installed as an administrator, its shortcuts for all (0590).
test-msiallusers:
	WINE=$(PREFIX)/bin/wine test/msiallusers-gate.sh

# A program's own right-click verbs show their names (0591).
test-verblabel:
	WINE=$(PREFIX)/bin/wine test/verblabel-gate.sh

# A window stays responsive while it copies (0592).
test-fileopresponsive:
	WINE=$(PREFIX)/bin/wine test/fileopresponsive-gate.sh

# Map network drive and Disconnect network drive (0593).
test-mapdrive:
	WINE=$(PREFIX)/bin/wine test/mapdrive-gate.sh

# Open with (0594).
test-openwith:
	WINE=$(PREFIX)/bin/wine test/openwith-gate.sh

# A big folder shows at once (0595).
test-folderspeed:
	WINE=$(PREFIX)/bin/wine test/folderspeed-gate.sh

# Create shortcut (0596).
test-shortcut:
	WINE=$(PREFIX)/bin/wine test/shortcut-gate.sh

# The taskbar is above the windows, behind a full-screen one (0597).
test-topbar:
	WINE=$(PREFIX)/bin/wine test/topbar-gate.sh

# An installer's feature tree shows its states, description and size (0598).
test-msiseltree:
	WINE=$(PREFIX)/bin/wine test/msiseltree-gate.sh

# File Explorer's status bar follows the view (0599).
test-statuscount:
	WINE=$(PREFIX)/bin/wine test/statuscount-gate.sh

# The folder view follows its folder (0601).
test-folderwatch:
	WINE=$(PREFIX)/bin/wine test/folderwatch-gate.sh

# Task View's windows fly into place (0520).
test-taskview-anim:
	WINE=$(PREFIX)/bin/wine test/taskview-anim-gate.sh

# Switching virtual desktops slides (0521).
test-desktop-slide:
	WINE=$(PREFIX)/bin/wine test/desktop-slide-gate.sh

# The Web Account Manager: a work account's sign-in through sg-wam-msal (0512,
# 1479), with a stub program standing in for it.
test-webauthcore:
	WINE=$(PREFIX)/bin/wine test/webauthcore-gate.sh

# wininet accepts the listen timeout Office's HTTP client sets (1477).
test-wininet-listentimeout:
	WINE=$(PREFIX)/bin/wine test/wininet-listentimeout-gate.sh

# The online identity API's ticket request is answered (1478).
test-onlineid-ticket:
	WINE=$(PREFIX)/bin/wine test/onlineid-ticket-gate.sh

# RichEdit's ITextServices2 and ITextDocument2 (1490): Word's React Native
# start screen ended in a fail-fast without them.
test-richtxsrv2:
	WINE=$(PREFIX)/bin/wine test/richtxsrv2-gate.sh

# A Direct2D bitmap shared from another bitmap can be drawn (1491): Word's
# ribbon font name and size boxes were black.
test-d2dsharedview:
	WINE=$(PREFIX)/bin/wine test/d2dsharedview-gate.sh

# combase's RoTransformError and RoOriginateErrorW return (1492): Word ended
# a minute after opening a document.
test-rotransformerror:
	WINE=$(PREFIX)/bin/wine test/rotransformerror-gate.sh

# Other processes' CPU times and cycle counts (1600).
test-cputime:
	WINE=$(PREFIX)/bin/wine test/cputime-gate.sh

# The file information classes Windows has (1601).
test-fileinfo:
	WINE=$(PREFIX)/bin/wine test/fileinfo-gate.sh

# Process and thread settings kept and read back (1602).
test-procsettings:
	WINE=$(PREFIX)/bin/wine test/procsettings-gate.sh

# CryptProtectMemory / RtlEncryptMemory encrypt (1603).
test-memprotect:
	WINE=$(PREFIX)/bin/wine test/memprotect-gate.sh

# Display affinity, user object security, power notifications (1604).
test-userapis:
	WINE=$(PREFIX)/bin/wine test/userapis-gate.sh

# Precise interrupt time, file streams, fail-fast, drivers, page files (1605).
test-kbmisc:
	WINE=$(PREFIX)/bin/wine test/kbmisc-gate.sh

# CPU sets, NUMA nodes, ideal processors, priority boost (1606).
test-cpusets:
	WINE=$(PREFIX)/bin/wine test/cpusets-gate.sh

# Error report registrations (1607).
test-werreg:
	WINE=$(PREFIX)/bin/wine test/werreg-gate.sh

# Device family version, ETW provider information, other threads' layout (1608).
test-devfamily:
	WINE=$(PREFIX)/bin/wine test/devfamily-gate.sh

# SetEntriesInAclW builds the ACL as Windows does (1609).
test-aclentries:
	WINE=$(PREFIX)/bin/wine test/aclentries-gate.sh

# A new RichEdit has no placeholder text (1610): Word crashed on exit.
test-richinit:
	WINE=$(PREFIX)/bin/wine test/richinit-gate.sh

# Buffered painting parameters, clear, set alpha (1611).
test-bufpaint:
	WINE=$(PREFIX)/bin/wine test/bufpaint-gate.sh

# dwmapi accent colour, attributes, blur behind (1612).
test-dwmattrs:
	WINE=$(PREFIX)/bin/wine test/dwmattrs-gate.sh

# GetFileMUIPath (1613).
test-muipath:
	WINE=$(PREFIX)/bin/wine test/muipath-gate.sh

# SHGetStockIconInfo flags (1614).
test-stockicon:
	WINE=$(PREFIX)/bin/wine test/stockicon-gate.sh

# EnumJobs lists a printer's queue; pages are counted (1615).
test-enumjobs:
	WINE=$(PREFIX)/bin/wine test/enumjobs-gate.sh

# ReplaceFile merges; power information; small stubs (1616).
test-replacefile:
	WINE=$(PREFIX)/bin/wine test/replacefile-gate.sh

# EnumDependentServices lists the dependents (1617).
test-svcdepend:
	WINE=$(PREFIX)/bin/wine test/svcdepend-gate.sh

# InitiateSystemShutdown/InitiateShutdown, abortably (1618).
test-sysshutdown:
	WINE=$(PREFIX)/bin/wine test/sysshutdown-gate.sh

# Privilege display names, Safer levels, security down a tree (1619).
test-advsec:
	WINE=$(PREFIX)/bin/wine test/advsec-gate.sh

# shcore: explicit AppUserModelID, scale factors and registrations, IsOS (1620).
test-shcorebits:
	WINE=$(PREFIX)/bin/wine test/shcorebits-gate.sh

test-fontquery:
	WINE=$(PREFIX)/bin/wine test/fontquery-gate.sh

test-guithread:
	WINE=$(PREFIX)/bin/wine test/guithread-gate.sh

test-ipnotify:
	WINE=$(PREFIX)/bin/wine test/ipnotify-gate.sh

test-apprestart:
	WINE=$(PREFIX)/bin/wine test/apprestart-gate.sh

test-minidumpx:
	WINE=$(PREFIX)/bin/wine test/minidumpx-gate.sh

test-joblimits:
	WINE=$(PREFIX)/bin/wine test/joblimits-gate.sh

test-fmtsupport:
	WINE=$(PREFIX)/bin/wine test/fmtsupport-gate.sh

test-printnotify:
	WINE=$(PREFIX)/bin/wine test/printnotify-gate.sh

test-diskinfo:
	WINE=$(PREFIX)/bin/wine test/diskinfo-gate.sh

test-perfcount:
	WINE=$(PREFIX)/bin/wine test/perfcount-gate.sh

test-taskdisp:
	WINE=$(PREFIX)/bin/wine test/taskdisp-gate.sh

test-taskinfo:
	WINE=$(PREFIX)/bin/wine test/taskinfo-gate.sh

test-taskxml:
	WINE=$(PREFIX)/bin/wine test/taskxml-gate.sh

test-bitsjob:
	WINE=$(PREFIX)/bin/wine test/bitsjob-gate.sh

test-bitsmore:
	WINE=$(PREFIX)/bin/wine test/bitsmore-gate.sh

test-mmcss:
	WINE=$(PREFIX)/bin/wine test/mmcss-gate.sh

test-devtree:
	WINE=$(PREFIX)/bin/wine test/devtree-gate.sh

test-pinterfaceiid:
	WINE=$(PREFIX)/bin/wine test/pinterfaceiid-gate.sh

test-taskprogress:
	WINE=$(PREFIX)/bin/wine test/taskprogress-gate.sh

test-winappid:
	WINE=$(PREFIX)/bin/wine test/winappid-gate.sh

test-childpolicy:
	WINE=$(PREFIX)/bin/wine test/childpolicy-gate.sh

test-smallstubs2:
	WINE=$(PREFIX)/bin/wine test/smallstubs2-gate.sh

test-asynclock:
	WINE=$(PREFIX)/bin/wine test/asynclock-gate.sh

test-uilangs:
	WINE=$(PREFIX)/bin/wine test/uilangs-gate.sh

test-shellapp:
	WINE=$(PREFIX)/bin/wine test/shellapp-gate.sh

test-shelldisp:
	WINE=$(PREFIX)/bin/wine test/shelldisp-gate.sh

test-wshshell:
	WINE=$(PREFIX)/bin/wine test/wshshell-gate.sh

test-textstreams:
	WINE=$(PREFIX)/bin/wine test/textstreams-gate.sh

test-fsoitems:
	WINE=$(PREFIX)/bin/wine test/fsoitems-gate.sh

test-wshnet:
	WINE=$(PREFIX)/bin/wine test/wshnet-gate.sh

test-vbsbuiltins:
	WINE=$(PREFIX)/bin/wine test/vbsbuiltins-gate.sh

test-vbsdyncode:
	WINE=$(PREFIX)/bin/wine test/vbsdyncode-gate.sh

test-jsstubs:
	WINE=$(PREFIX)/bin/wine test/jsstubs-gate.sh

test-nsplookup:
	WINE=$(PREFIX)/bin/wine test/nsplookup-gate.sh

test-wscatalog:
	WINE=$(PREFIX)/bin/wine test/wscatalog-gate.sh

test-gdipquality:
	WINE=$(PREFIX)/bin/wine test/gdipquality-gate.sh

test-hotkeyexec:
	WINE=$(PREFIX)/bin/wine test/hotkeyexec-gate.sh

# A locale's sort looked up re-entrantly (0513): App-V's registry hooks.
test-sortreenter:
	WINE=$(PREFIX)/bin/wine test/sortreenter-gate.sh

# cldapi.dll (Cloud Files API) and cryptxml.dll (XML digital signatures),
# the two modules Microsoft OneDrive's sync client imports that Wine lacked;
# without them OneDrive.exe cannot load its sync stack (0518).
test-onedrive-dlls:
	WINE=$(PREFIX)/bin/wine test/onedrive-dlls-gate.sh

# Service trigger info, ChangeServiceConfig2/QueryServiceConfig2 at
# SERVICE_CONFIG_TRIGGER_INFO: OneDrive's per-machine install registers its
# Updater Service with one (0523).
test-svctrigger:
	WINE=$(PREFIX)/bin/wine test/svctrigger-gate.sh

# UISettings' values and Changed events, CoreWindow.GetForCurrentThread():
# OneDrive ended at start on their stubs (0524).
test-uisettings:
	WINE=$(PREFIX)/bin/wine test/uisettings-gate.sh

# Shell_NotifyIconGetRect: where a notification-area icon is (0525).
test-notifyiconrect:
	WINE=$(PREFIX)/bin/wine test/notifyiconrect-gate.sh

# GetShellWindow() on the shell's desktop is explorer's (0526).
test-shellwindow:
	WINE=$(PREFIX)/bin/wine test/shellwindow-gate.sh

# One Service Control Manager: a second services.exe leaves (0530).
test-scmsingle:
	WINE=$(PREFIX)/bin/wine test/scmsingle-gate.sh

# X stacking follows Wine's Z order in a virtual desktop (0531).
test-xstack:
	WINE=$(PREFIX)/bin/wine test/xstack-gate.sh

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

# Selecting and dragging: the desktop's rubber band, icons dragged on the desktop,
# into folders and File Explorer; File Explorer's rubber band and drags (0630-0632).
test-drag:
	WINE=$(PREFIX)/bin/wine test/drag-gate.sh

test-deskhover:
	WINE=$(PREFIX)/bin/wine test/deskhover-gate.sh

# the desktop's icons after an erase-only repaint, the shell's start after a look change (1127)
test-deskerase:
	WINE=$(PREFIX)/bin/wine test/deskerase-gate.sh

# a thread woken from an idle wait is not taken for hung (1128)
test-hungwake:
	WINE=$(PREFIX)/bin/wine test/hungwake-gate.sh

# Save As takes the name as typed: the suggestion selected, a typed type kept (1129)
test-savename:
	WINE=$(PREFIX)/bin/wine test/savename-gate.sh

# a new display scale while programs run; an appbar docks at once (1120-1126)
test-dpifollow:
	WINE=$(PREFIX)/bin/wine test/dpifollow-gate.sh

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

# Symbolic links and junctions are real (0360): the EA app's installer.
test-symlink:
	WINE=$(PREFIX)/bin/wine test/symlink-gate.sh

# A program's compatibility settings apply when it starts (0420): the
# Environment, LaunchArgs and Version the Compatibility tab stores. RUNASADMIN
# is in test-elevreq (it needs a user who is not elevated).
test-appcompat:
	WINE=$(PREFIX)/bin/wine test/appcompat-gate.sh

# ucrtbase imaxdiv and the wide intmax conversions exist (0421): 64/32-bit,
# ucrtbase, msvcr120, msvcr120_app.
test-crtimax:
	WINE=$(PREFIX)/bin/wine test/crtimax-gate.sh

# A named stream (file:Zone.Identifier) is refused as on FAT, not made a
# second file; file::$$DATA is the file (0422).
test-streams:
	WINE=$(PREFIX)/bin/wine test/streams-gate.sh

# GetCurrentApplicationUserModelId / GetApplicationUserModelId (0423):
# Thunderbird 128 died on the missing export.
test-aumid:
	WINE=$(PREFIX)/bin/wine test/aumid-gate.sh

# IDXGIKeyedMutex on a SHARED_KEYEDMUTEX texture with Wine's own d3d11
# (0424). Needs Xvfb.
test-keyedmutex:
	WINE=$(PREFIX)/bin/wine test/keyedmutex-gate.sh

# What Zoom and Teams need at start: SetThreadpoolTimerEx/WaitEx,
# Windows.ApplicationModel.LimitedAccessFeatures (0425-0426).
test-meetings:
	WINE=$(PREFIX)/bin/wine test/meetings-gate.sh

# Direct2D geometries: area, length, containment, relations, arcs, and a
# command list's bounds (0427); Paint.NET brush masks (0429, 0430). Needs Xvfb.
test-d2dgeom:
	WINE=$(PREFIX)/bin/wine test/d2dgeom-gate.sh

# msvcp140's mutex and condition variable in the layout Visual Studio 2022
# 17.10+ constructs inline (0428).
test-stlmtx:
	WINE=$(PREFIX)/bin/wine test/stlmtx-gate.sh

# Image pages shared between processes when sections are not page-aligned in
# the file (0431: Chromium/Electron DLLs); huge PAGE_NOACCESS reservations cost
# no memory (0432: V8/PartitionAlloc cages).
test-imgshare:
	WINE=$(PREFIX)/bin/wine test/imgshare-gate.sh

test-bigreserve:
	WINE=$(PREFIX)/bin/wine test/bigreserve-gate.sh

# NtQueryWnfStateData: Thunderbird's new-mail notification (0433).
test-wnf:
	WINE=$(PREFIX)/bin/wine test/wnf-gate.sh

# AppContainer SID names: Chrome's sandbox quit the browser without them (0434).
test-appcontainer:
	WINE=$(PREFIX)/bin/wine test/appcontainer-gate.sh

# Sheet-of-glass windows keep their alpha: Firefox popups had black frames (0435).
test-glass:
	WINE=$(PREFIX)/bin/wine test/glass-gate.sh

# 0605: a composition swap chain with alpha shows its alpha (Chrome's menus).
test-dcompalpha:
	WINE=$(PREFIX)/bin/wine test/dcompalpha-gate.sh

# Toast notifications and their XML documents (0436-0437): Firefox, Chrome,
# Electron and .NET programs show them; clicks and dismissals reach the program.
test-toast:
	WINE=$(PREFIX)/bin/wine test/toast-gate.sh

# shutdown.exe (0438): /s /r /p /l /t /c, as scripts and the Ctrl+Alt+Del screen use it.
test-shutdownexe:
	WINE=$(PREFIX)/bin/wine test/shutdownexe-gate.sh

# Signing out ends the shell, so the session ends (0439). Needs Xvfb.
test-logoff:
	WINE=$(PREFIX)/bin/wine test/logoff-gate.sh

# cmd's banner and console title (0440).
test-cmdbanner:
	WINE=$(PREFIX)/bin/wine test/cmdbanner-gate.sh

# The default window icon is our mark, not Wine's glass (0441).
test-winlogo:
	WINE=$(PREFIX)/bin/wine test/winlogo-gate.sh

# A changed tray icon does not show the old one through it (0442). Needs Xvfb.
test-trayicon:
	WINE=$(PREFIX)/bin/wine test/trayicon-gate.sh

# The Desktop namespace has no "/" (0443).
test-nsroot:
	WINE=$(PREFIX)/bin/wine test/nsroot-gate.sh

# A battery reporting energy reads right (0445). Needs sudo -n (a mount namespace).
test-battery:
	WINE=$(PREFIX)/bin/wine test/battery-gate.sh

# Shared D3D11 textures under DXVK do not crash (0361): GOG Galaxy, browser
# engines. Needs DXVK_DIR (a DXVK build with x64/d3d11.dll) and Xvfb.
test-d3dshared:
	WINE=$(PREFIX)/bin/wine test/d3dshared-gate.sh

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
# Direct2D layers, primitive blends, DrawImage composite modes (0340);
# effect graphs as Paint.NET builds them (0341); a display's colour
# capabilities (0342). Xvfb.
test-d2dlayer:
	WINE=$(PREFIX)/bin/wine test/d2dlayer-gate.sh

test-d2dsvg:
	WINE=$(PREFIX)/bin/wine test/d2dsvg-gate.sh

test-kbchar:
	WINE=$(PREFIX)/bin/wine test/kbchar-gate.sh

test-d2dgraph:
	WINE=$(PREFIX)/bin/wine test/d2dgraph-gate.sh

test-advcolor:
	WINE=$(PREFIX)/bin/wine test/advcolor-gate.sh

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

test-iconhandler:
	WINE=$(PREFIX)/bin/wine test/iconhandler-gate.sh

test-gdipfamily:
	WINE=$(PREFIX)/bin/wine test/gdipfamily-gate.sh

test-dirshare:
	WINE=$(PREFIX)/bin/wine test/dirshare-gate.sh

# A Linux program drawing its own title bar gets a frame without one, moves
# and sizes it itself, and a new Linux window is framed soon (0803).
test-ownframe:
	WINE=$(PREFIX)/bin/wine test/ownframe-gate.sh

# A click on a maximized own-title-bar Linux program leaves the pointer free (0818).
test-embedmaxgrab:
	WINE=$(PREFIX)/bin/wine test/embedmaxgrab-gate.sh

# A click in the notification centre reaches the program that sent it
# (0819-0821): its COM activator, its toast's Activated event, its tray icon.
test-toastactivate:
	WINE=$(PREFIX)/bin/wine test/toastactivate-gate.sh

# The .NET Framework 3.5 as an optional feature (0827-0829): WMI's
# Win32_OptionalFeature, dism /get-featureinfo /enable-feature, fondue.exe.
test-netfx3:
	WINE=$(PREFIX)/bin/wine test/netfx3-gate.sh

# A share's folder read and shown at once (0822-0825): no request per file,
# volume information, drawn once with its icons, a crashed File Explorer
# forgotten, network drives not polled. Needs sudo (a stand-in share).
test-smbshare:
	WINE=$(PREFIX)/bin/wine test/smbshare-gate.sh
test-smbview:
	WINE=$(PREFIX)/bin/wine test/smbview-gate.sh

# NET USE lists the connections; network drives named for their folders
# (0826, 0838). Needs sudo.
test-netuse:
	WINE=$(PREFIX)/bin/wine test/netuse-gate.sh

# Map Network Drive asks for a name and password when a share refuses
# (0839). Needs sudo.
test-mapcred:
	WINE=$(PREFIX)/bin/wine test/mapcred-gate.sh

# AnyDesk (0860-0862): a desktop says whether it takes input (UOI_IO); a
# WinEvent hook skipping its own process skips the one that set it; the
# taskbar is not made active when the active window goes away.
test-inputdesk:
	WINE=$(PREFIX)/bin/wine test/inputdesk-gate.sh
test-winevskip:
	WINE=$(PREFIX)/bin/wine test/winevskip-gate.sh
test-noactnext:
	WINE=$(PREFIX)/bin/wine test/noactnext-gate.sh

# A pen's pressure and tilt to Windows programs, two-finger scrolling (1000,
# 1001); builds sg-compositor's test build from SG_COMPOSITOR_SRC (default:
# the sg-compositor checkout beside this one).
test-pentouch:
	WINE=$(PREFIX)/bin/wine test/pentouch-gate.sh

# Touches as touches (WM_POINTER of PT_TOUCH, WM_TOUCH, WM_GESTURE, the mouse
# for programs leaving them to DefWindowProc), edge swipes, a pen plugged in
# later and leaving range (1150); sg-compositor 0.2.0+sg40's test build from
# SG_COMPOSITOR_SRC.
test-touch:
	WINE=$(PREFIX)/bin/wine test/touch-gate.sh

# The taskbar's menu: "Show touch keyboard button" (1151).
test-keybutton:
	WINE=$(PREFIX)/bin/wine test/keybutton-gate.sh

# DirectWrite has the families FontSubstitutes name (1220): Skia's text
# (DYMO Connect's labels) in the font the screen shows.
test-dwsubst:
	WINE=$(PREFIX)/bin/wine test/dwsubst-gate.sh

# A printer's PrintCapabilities are its driver's, tickets name its papers
# (1221); its status is its CUPS queue's (1222); a queue CUPS had when a
# prefix was made is a printer of it (0924).
test-ptcaps:
	WINE=$(PREFIX)/bin/wine test/ptcaps-gate.sh

# Microsoft Edge 154 (David's Latitude 2026-10-06): a hidden, maximized window
# stays hidden on SC_MAXIMIZE (1300); the type library marshaler for bases
# with no proxy/stub of their own (1301); a class served by a service starts
# for a standard user, no needless 30-second surrogate wait (1302, needs the
# sgconf standard user); the diagnostic-data/kiosk settings classes (1303).
test-hiddenmax:
	WINE=$(PREFIX)/bin/wine test/hiddenmax-gate.sh

test-tlbbase:
	WINE=$(PREFIX)/bin/wine test/tlbbase-gate.sh

test-comsvc:
	WINE=$(PREFIX)/bin/wine bash test/comsvc-gate.sh

test-privacysettings:
	WINE=$(PREFIX)/bin/wine test/privacysettings-gate.sh

test-dwspacing:
	WINE=$(PREFIX)/bin/wine test/dwspacing-gate.sh

test-msgboxicons:
	WINE=$(PREFIX)/bin/wine test/msgboxicons-gate.sh

# Credential Manager keeps credentials in the person's keyring (1380):
# secret-tool finds what CredWrite wrote; round trips, migration, fallbacks.
test-credkeyring:
	WINE=$(PREFIX)/bin/wine test/credkeyring-gate.sh

# A pen that comes after a program started, and goes again (1420):
# WM_POINTERDEVICECHANGE, Wintab finding a late tablet, a pen gone leaving its
# window; sg-compositor 0.2.0+sg46's test tablet ("plug", "unplug") from
# SG_COMPOSITOR_SRC.
test-latepen:
	WINE=$(PREFIX)/bin/wine test/latepen-gate.sh

test-zipfolder:
	WINE=$(PREFIX)/bin/wine test/zipfolder-gate.sh

test-thumbbar:
	WINE=$(PREFIX)/bin/wine test/thumbbar-gate.sh

test-xplacement:
	WINE=$(PREFIX)/bin/wine test/xplacement-gate.sh

test-tsfsinks:
	WINE=$(PREFIX)/bin/wine test/tsfsinks-gate.sh

test-certresync:
	WINE=$(PREFIX)/bin/wine test/certresync-gate.sh

test-inputpane:
	WINE=$(PREFIX)/bin/wine test/inputpane-gate.sh

test-jumplist:
	WINE=$(PREFIX)/bin/wine test/jumplist-gate.sh

test-evtquery:
	WINE=$(PREFIX)/bin/wine test/evtquery-gate.sh

test-audiotopo:
	WINE=$(PREFIX)/bin/wine test/audiotopo-gate.sh

test-dxcore:
	WINE=$(PREFIX)/bin/wine test/dxcore-gate.sh

test-dmanip:
	WINE=$(PREFIX)/bin/wine test/dmanip-gate.sh

test-imglist2:
	WINE=$(PREFIX)/bin/wine test/imglist2-gate.sh

test-lvtv:
	WINE=$(PREFIX)/bin/wine test/lvtv-gate.sh

test-toolbar2:
	WINE=$(PREFIX)/bin/wine test/toolbar2-gate.sh

test-ccmisc:
	WINE=$(PREFIX)/bin/wine test/ccmisc-gate.sh

test-ole32misc:
	WINE=$(PREFIX)/bin/wine test/ole32misc-gate.sh

test-oapict:
	WINE=$(PREFIX)/bin/wine test/oapict-gate.sh

test-tlbcreate:
	WINE=$(PREFIX)/bin/wine test/tlbcreate-gate.sh

test-roerror:
	WINE=$(PREFIX)/bin/wine test/roerror-gate.sh

test-guires:
	WINE=$(PREFIX)/bin/wine test/guires-gate.sh

test-powerreq:
	WINE=$(PREFIX)/bin/wine test/powerreq-gate.sh

test-ncryptksp:
	WINE=$(PREFIX)/bin/wine test/ncryptksp-gate.sh

test-cryptrest:
	WINE=$(PREFIX)/bin/wine test/cryptrest-gate.sh

test-etwtrace:
	WINE=$(PREFIX)/bin/wine test/etwtrace-gate.sh

test-uiaevents:
	WINE=$(PREFIX)/bin/wine test/uiaevents-gate.sh

test-msgfilter:
	WINE=$(PREFIX)/bin/wine test/msgfilter-gate.sh

test-uiabridge:
	WINE=$(PREFIX)/bin/wine test/uiabridge-gate.sh

test-shellmisc:
	WINE=$(PREFIX)/bin/wine test/shellmisc-gate.sh

test-oleaccstd:
	WINE=$(PREFIX)/bin/wine test/oleaccstd-gate.sh

test-sockmisc:
	WINE=$(PREFIX)/bin/wine test/sockmisc-gate.sh

test-netlist:
	WINE=$(PREFIX)/bin/wine test/netlist-gate.sh
