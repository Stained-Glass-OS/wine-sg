#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# LDAP_OPT_SEND_TIMEOUT in wldap32 (patches/sg/1527). Windows takes the
# time allowed for sending a request (an l_timeval) and gives it back. Office
# sets it, with signing and sealing, on the connection it opens to the
# global catalog (port 3268) as it starts; Wine answered LDAP_NOT_SUPPORTED
# and Office gave up before it connected, where Windows goes on to find
# there is no domain. No server is contacted by the probe.
#
#   WINE=/opt/wine-sg/bin/wine test/ldapsendtimeout-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ldapsendtimeout.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/ldapsendtimeout-probe.c" -lwldap32 || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/ldapsendtimeout-probe.c" -lwldap32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in 64 32; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r')
    echo "      $a: $O"
    case "$O" in "init=1 set=0 get=0 sec=20 usec=500000 "*) pass "$a-bit: the send timeout is taken and given back" ;; *) fail "$a-bit set/get: $O" ;; esac
    case "$O" in *" setA=0 null=89") pass "$a-bit: the A function takes it too; NULL is a parameter error" ;; *) fail "$a-bit A/NULL: $O" ;; esac
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
