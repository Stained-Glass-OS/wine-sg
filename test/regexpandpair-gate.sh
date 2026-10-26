#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RegGetValue accepts RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ without RRF_NOEXPAND (patches/sg/7004): a
# REG_EXPAND_SZ value is then returned expanded, as REG_SZ. Only REG_EXPAND_SZ alone without
# RRF_NOEXPAND is invalid. Python's launcher py.exe reads PythonCore\X.Y\InstallPath so and got
# ERROR_INVALID_PARAMETER: "No installed Python found!" after a normal Python install.
# The probe is test/regexpandpair-probe.c, 64- and 32-bit (py.exe is 32-bit).
#   WINE=/opt/wine-sg/bin/wine test/regexpandpair-gate.sh
# Mutant: SG_MUTANT_GETVALUE_EXPAND_PAIR (dlls/kernelbase/registry.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-regexpand.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/probe$b.exe" "$HERE/regexpandpair-probe.c" -ladvapi32 || { fail "probe ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    out=$(timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$b-bit: $2"
        else fail "$b-bit: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check pair_size_ok         "REG_SZ|REG_EXPAND_SZ without NOEXPAND: the size of a REG_SZ value (py.exe's call)"
    check pair_sz_ok           "...and its data, as REG_SZ"
    check pair_expanded_ok     "...a REG_EXPAND_SZ value comes back expanded, as REG_SZ"
    check expand_alone_invalid "REG_EXPAND_SZ alone without NOEXPAND is still ERROR_INVALID_PARAMETER"
    check pair_ansi_ok         "RegGetValueA accepts the pair too"
    check done                 "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
