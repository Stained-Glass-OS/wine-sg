#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# gdi32's GPU scheduling priority class (patches/sg/0633). OBS Studio imports
# D3DKMTSetProcessSchedulingPriorityClass from gdi32 to raise its renderer's
# GPU priority; Wine did not export it, so the import became a stub and OBS
# aborted right after "D3D11 loaded successfully" ("Call ... to unimplemented
# function GDI32.dll.D3DKMTSetProcessSchedulingPriorityClass"). Now both the
# Set and Get calls are exported: this process's class is kept and read back,
# a class past REALTIME and a missing output are STATUS_INVALID_PARAMETER, a
# handle that is no process STATUS_INVALID_HANDLE. 64-bit and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/gpupriority-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for a in x86_64 i686; do
    command -v "$a-w64-mingw32-gcc" >/dev/null || { echo "SKIP: $a mingw-w64 not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-gpupriority.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp "$a-w64-mingw32-gcc" -O2 -o "$T/gp-$a.exe" "$HERE/gpupriority-probe.c" ||
        { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/gp-*.exe "$WINEPREFIX/drive_c/"
v() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
for a in x86_64 i686; do
    out=$(timeout 60 "$WINE" "C:\\gp-$a.exe" 2>/dev/null | tr -d '\r')
    [ "$(v "$out" exports)" = 2 ] && pass "$a: gdi32 exports Set/GetProcessSchedulingPriorityClass" ||
        { fail "$a: exports $(v "$out" exports)"; continue; }
    [ "$(v "$out" set)" = 00000000 ] && [ "$(v "$out" get)" = 00000000 ] && [ "$(v "$out" class)" = 4 ] &&
        pass "$a: HIGH is set and read back" ||
        fail "$a: set $(v "$out" set) get $(v "$out" get) class $(v "$out" class)"
    [ "$(v "$out" invalid)" = c000000d ] && [ "$(v "$out" null)" = c000000d ] && [ "$(v "$out" badhandle)" = c0000008 ] &&
        pass "$a: a bad class, no output and a bad handle are refused" ||
        fail "$a: invalid $(v "$out" invalid) null $(v "$out" null) badhandle $(v "$out" badhandle)"
done
exit $RC
