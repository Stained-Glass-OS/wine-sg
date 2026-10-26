#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HKCU\Software is one key for 32- and 64-bit programs (6110). Windows shares HKCU\Software between
# the views since Windows 7 (only Software\Classes is redirected). A 64-bit program opening keys
# with KEY_WOW64_32KEY went into HKCU\Software\Wow6432Node once one existed: Edge WebView2's
# user-level setup (64-bit Chromium setup) wrote its EdgeUpdate\Clients key there and the 32-bit
# updater found none (0x80040905), so WebView2 -- and CCleaner, which needs it -- did not install.
# test/hkcuwow32-probe.c: 64-bit writes with KEY_WOW64_32KEY, 32-bit reads.
#   WINE=/opt/wine-sg/bin/wine test/hkcuwow32-gate.sh
# Mutant: SG_MUTANT_HKCU_WOW32KEY (dlls/kernelbase/registry.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $cc >/dev/null || { echo "SKIP: $cc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hkcuwow32.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/hkcuwow32-probe.c" -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/hkcuwow32-probe.c" -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
RC=0
w=$(timeout -s KILL 60 "$WINE" "$T/probe64.exe" write 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$w" | sed 's/^/      /'
r=$(timeout -s KILL 60 "$WINE" "$T/probe32.exe" read 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$r" | sed 's/^/      /'
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
printf '%s\n' "$w" | grep -qx 'CREATE 0' && pass "a 64-bit program creates HKCU\\Software\\SGTestWow\\Clients with KEY_WOW64_32KEY" || fail "create"
printf '%s\n' "$w" | grep -qx 'UNDERWOW 2' && pass "it is not under HKCU\\Software\\Wow6432Node" || fail "under Wow6432Node: $w"
printf '%s\n' "$w" | grep -qx 'REOPEN32 0' && pass "the 64-bit program opens it again with KEY_WOW64_32KEY" || fail "reopen"
printf '%s\n' "$r" | grep -qx 'READ 0 155' && pass "a 32-bit program reads it and its value" || fail "32-bit read: $r"
printf '%s\n' "$w" | grep -qx 'HKLMWOW 0' && pass "HKLM\\Software keeps its redirection: KEY_WOW64_32KEY goes to Wow6432Node there" || fail "HKLM: $w"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
