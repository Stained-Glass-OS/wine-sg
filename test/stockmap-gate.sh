#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SHGetStockIconInfo's icons (patches/sg/9819): shell32's own drawings at the
# resource ids Windows' shell32 gives them (the stock id + 1) -- the Print
# dialog's printers (PrintDlgEx shows SIID_PRINTER), folders, drives, the
# network, the recycle bin, shortcuts. Every stock icon but four was the
# plain file. test/stockmap-probe.c, 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/stockmap-gate.sh
# Mutant: SG_MUTANT_STOCK_PLAIN (shell32/iconcache.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-stockmap.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0
for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -O2 -o "$T/probe${m#*:}.exe" "$HERE/stockmap-probe.c" -lshell32 || { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    if printf '%s\n' "$out" | grep -q '^RESULT: PASS'; then echo "PASS  $b-bit: the stock icons are shell32's drawings of them"
    else echo "FAIL  $b-bit: the stock icons"; RC=1; fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
