#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A child process's DLL search path has its parent's DLL directory (patches/sg/9757). Windows computes
# a new process's search path in the parent, with the directory the parent gave SetDllDirectory;
# Wireshark sets its program folder so its capture helpers in extcap\ find libwsutil.dll and GLib.
# Wine's children computed their own path ("Library libwsutil.dll (which is needed by ...\extcap\
# etwdump.exe) not found"). The probe (test/dlldirchild-probe.c) starts a child that imports a DLL
# from another folder:
#   with no DLL directory it does not start (STATUS_DLL_NOT_FOUND, as on Windows)
#   after SetDllDirectory(that folder) it starts and runs (exit 42)
#   WINE=/opt/wine-sg/bin/wine test/dlldirchild-gate.sh
# Mutant (kernelbase/process.c): SG_MUTANT_NO_INHERITED_DLL_DIRECTORY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dlldirchild.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
CC=x86_64-w64-mingw32-gcc
$CC -O1 -shared -DHELPER -o "$T/helper.dll" -Wl,--out-implib,"$T/libhelper.a" "$HERE/dlldirchild-probe.c" &&
$CC -O1 -DCHILD -o "$T/child.exe" "$HERE/dlldirchild-probe.c" -L"$T" -lhelper &&
$CC -O1 -o "$T/probe.exe" "$HERE/dlldirchild-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/dd/app" "$C/dd/lib" && cp "$T/child.exe" "$C/dd/app/" && cp "$T/helper.dll" "$C/dd/lib/"
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
l() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
[ "$(l PLAIN)" = c0000135 ] && pass "no DLL directory: the child does not find the DLL (STATUS_DLL_NOT_FOUND)" || fail "plain: '$(l PLAIN)'"
[ "$(l DLLDIR)" = 0000002a ] && pass "the parent's DLL directory: the child finds it and runs" || fail "with the DLL directory: '$(l DLLDIR)' (want 0000002a)"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
