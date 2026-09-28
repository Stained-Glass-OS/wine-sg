#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The administrative tools' launchers carry icons (patches/sg/0463).
#
# mmc.exe, eventvwr.exe, resmon.exe and cleanmgr.exe hand off to the shell's
# consoles, but had no icon of their own: the Start menu's Computer
# Management, Device Manager, Disk Cleanup and Event Viewer, and every .msc
# file (whose icon is mmc.exe's), showed the generic program icon.
#
#   WINE=/opt/wine-sg/bin/wine test/admintoolicons-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-admintoolicons.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/admintoolicons-probe.exe" "$HERE/admintoolicons-probe.c" -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" "$T/admintoolicons-probe.exe" 2>/dev/null | tr -d '\r')
echo "$out" | sed 's/^/      /'
for p in mmc eventvwr resmon cleanmgr; do
    n=$(echo "$out" | sed -n "s/^$p.exe //p")
    [ "${n:-0}" -ge 1 ] 2>/dev/null && [ "$n" -le 64 ] && pass "$p.exe carries an icon" || fail "$p.exe has no icon"
done
echo "$out" | grep -qx "msc-is-mmc 1" && pass "a .msc file shows mmc.exe's icon (Computer Management, Services...)" || fail "a .msc file's icon is not mmc.exe's"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
