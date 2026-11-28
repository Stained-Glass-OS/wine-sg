#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# imageres.dll, the system's images (patches/sg/9635, i1012). Programs load
# shared icons from it by resource id -- the elevation shield, documents,
# folders, the libraries -- and Wine had none: every such load failed, and
# WireGuard for Windows' window, which takes the icon it asked for as given,
# ended with a nil pointer panic. Also shell32's -16739 (a change under way).
# Each icon loads through SHDefExtractIcon of system32\<dll>.dll at 16 pixels.
# 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/imageres-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WINESERVER=$(sg_wineserver); export WINESERVER
T=$(mktemp -d /var/tmp/sg-imageres.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
sg_gate_display || exit 77
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/imageres-probe.c" -lshell32 -luser32 -lgdi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 60 "$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    bad=$(printf '%s\n' "$out" | awk '$1 == "ICON" && ($4 != 1 || $5 != 16)')
    n=$(printf '%s\n' "$out" | grep -c '^ICON .* 1 16$')
    [ -z "$bad" ] && [ "$n" = 15 ] && echo "      PASS  all 15 icons load at 16 pixels (imageres 1, 2, 3, 4, 78, 106, 174, 1002-1005, 1028; shell32 16739, 258, 240)" \
        || { echo "      FAIL  icons that did not load:"; printf '%s\n' "${bad:-$out}" | sed 's/^/          /'; RC=1; }
    c=$(printf '%s\n' "$out" | sed -n 's/^COUNT //p')
    [ "${c:-0}" -ge 12 ] && echo "      PASS  imageres.dll has its icons ($c)" || { echo "      FAIL  imageres.dll icon count: ${c:-none}"; RC=1; }
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
