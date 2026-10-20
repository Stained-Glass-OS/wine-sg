#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ldap_init with an empty host name opens the default host (patches/sg/5939).
#
# Word calls ldap_initW(L"", 3268) at startup. Joining no host names gave a
# zero-byte, unterminated URL, so ldap_init read heap garbage: it failed when
# the heap held the wrong bytes and Word crashed seconds after starting.
# Heap checking (warn+heap fills new blocks with 0xab) makes the old code fail
# every time.
#
#   WINE=/opt/wine-sg/bin/wine test/ldapemptyhost-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ldapemptyhost.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/ldapemptyhost-probe.c" -lwldap32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(WINEDEBUG=warn+heap timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
case "$out" in
*ALL-OK*) echo "PASS  ldap_init with no host name opens the default host"; echo "RESULT: PASS"; exit 0 ;;
*) echo "FAIL  an empty host name gave no session"; echo "RESULT: FAIL"; exit 1 ;;
esac
