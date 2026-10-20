#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An empty machine Root store holds the host's roots (patches/sg/5940).
#
# Only an administrator writes HKLM\...\SystemCertificates\Root; on a fresh
# machine no administrator's program had opened it, so it was empty and a
# standard user's programs trusted no HTTPS server (SG Store: 12045/12057).
# The probe empties the key and denies writing it, then a new program counts
# the roots it trusts.
#
#   WINE=/opt/wine-sg/bin/wine test/hostroots-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ls /etc/ssl/certs/*.pem >/dev/null 2>&1 || { echo "SKIP: no host CA certificates"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hostroots.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/hostroots-probe.c" -lcrypt32 -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# registry security descriptors live in the running server only: keep one up
"$WINESERVER" -p
lock=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" lock 2>/dev/null | tr -d '\r')
printf '%s\n' "$lock" | sed 's/^/      /'
case "$lock" in *"LOCKED (write open now denied)"*) ;; *) echo "FAIL  could not lock the machine Root store"; echo "RESULT: FAIL"; exit 1 ;; esac
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" count 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
fail=0
case "$out" in *"registry roots=0"*) ;; *) echo "FAIL  the locked registry store was written"; fail=1 ;; esac
case "$out" in *"user roots="[1-9]*"isrg=1"*) echo "PASS  a program trusts the host's roots while the machine store is empty" ;;
  *) echo "FAIL  no roots (or no ISRG Root X1) for a program while the machine store is empty"; fail=1 ;; esac
case "$out" in *"machine roots="[1-9]*"isrg=1"*) echo "PASS  the machine's Root store includes them" ;;
  *) echo "FAIL  the machine's Root store lacks them"; fail=1 ;; esac
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
