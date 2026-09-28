#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder's right-click menu has New > Folder, Shortcut and Text Document,
# and Text Document makes "New Text Document.txt", then "(2)" beside it
# (patches/sg/0450; field report 2: only New Folder / New Link).
#
#   WINE=/opt/wine-sg/bin/wine test/newtext-gate.sh
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
T=$(mktemp -d /var/tmp/sg-newtext.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/newtext-probe.exe" "$HERE/newtext-probe.c" -lole32 -lshell32 -luser32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/folder"
out=$("$WINE" "$T/newtext-probe.exe" "Z:$(printf '%s' "$T/folder" | tr / '\\')" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
{ printf '%s\n' "$out" | grep -qx 'new: &Folder' && printf '%s\n' "$out" | grep -qx 'new: &Shortcut' \
    && printf '%s\n' "$out" | grep -qx 'new: &Text Document'; } && pass "New: Folder, Shortcut, Text Document" || fail "the New submenu"
{ [ -f "$T/folder/New Text Document.txt" ] && [ -f "$T/folder/New Text Document (2).txt" ] && [ ! -s "$T/folder/New Text Document.txt" ]; } \
    && pass "Text Document makes an empty New Text Document.txt, then (2)" || fail "files: $(ls "$T/folder")"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
