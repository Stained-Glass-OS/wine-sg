#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# kernel32's process snapshot API, PssCaptureSnapshot and the rest
# (patches/sg/0620). They were not exported, and a program whose imports are
# resolved by its own code stopped: BleachBit's UPX-packed python312.dll
# looks up PssQuerySnapshot as it starts (CPython's os.getppid uses it), its
# DllMain failed, and BleachBit died in a stack overflow. A snapshot holds the
# process's information; its parent's id is what os.getppid reports. Both a
# 64-bit and a 32-bit (WoW64) program.
#
#   WINE=/opt/wine-sg/bin/wine test/pss-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-pss.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    "$a-w64-mingw32-gcc" -O2 -municode -o "$T/pss-$a.exe" "$HERE/pss-probe.c" ||
        { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/pss-*.exe "$WINEPREFIX/drive_c/"
v() { printf '%s\n' "$1" | sed -n "s/^$2=//p"; }
for a in x86_64 i686; do
    out=$(timeout 60 "$WINE" "C:\\pss-$a.exe" parent 2>/dev/null | tr -d '\r')
    [ "$(v "$out" exports)" = 10/10 ] && pass "$a: kernel32 exports the ten Pss functions" ||
        { fail "$a: exports $(v "$out" exports)"; continue; }
    [ "$(v "$out" capture)" = 0 ] && [ "$(v "$out" query)" = 0 ] && pass "$a: a snapshot is taken and queried" ||
        fail "$a: capture $(v "$out" capture) query $(v "$out" query)"
    pid=$(v "$out" pid)
    [ "${pid%% *}" = "${pid##*self=}" ] && pass "$a: its process id is the program's" || fail "$a: pid $pid"
    [ -n "$(v "$out" ppid)" ] && [ "$(v "$out" ppid)" = "$(v "$out" expected-ppid)" ] &&
        pass "$a: the parent's id is the parent's ($(v "$out" ppid))" ||
        fail "$a: parent $(v "$out" ppid), expected $(v "$out" expected-ppid)"
    case "$(v "$out" image)" in *pss-$a.exe) pass "$a: the image name" ;; *) fail "$a: image '$(v "$out" image)'" ;; esac
    [ "$(v "$out" short)" = 24 ] && [ "$(v "$out" threads)" = 1168 ] && [ "$(v "$out" walk)" = 259 ] && [ "$(v "$out" free)" = 0 ] &&
        pass "$a: a short buffer, an uncaptured class and the walk's end are errors; the snapshot is freed" ||
        fail "$a: short $(v "$out" short) threads $(v "$out" threads) walk $(v "$out" walk) free $(v "$out" free)"
done
exit $RC
