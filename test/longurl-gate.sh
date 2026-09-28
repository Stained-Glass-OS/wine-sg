#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ShellExecute of a URL longer than MAX_PATH reaches its handler whole
# (patches/sg/0472).
#
# SHELL_FindExecutable copied the "file" -- the whole URL -- into a MAX_PATH
# buffer on the stack to look for it on disk: a mailto: link with a body of a
# few thousand characters overran it and the caller crashed (Report a
# problem's Email button).
#
#   WINE=/opt/wine-sg/bin/wine test/longurl-gate.sh
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

T=$(mktemp -d /var/tmp/sg-longurl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/longurl-probe.exe" "$HERE/longurl-probe.c" -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
K='HKLM\Software\Classes\sglongurl'
"$WINE" reg add "$K" /ve /d "URL:sglongurl" /f >/dev/null 2>&1
"$WINE" reg add "$K" /v "URL Protocol" /d "" /f >/dev/null 2>&1
"$WINE" reg add "$K\\shell\\open\\command" /ve /d "\"Z:${T}/longurl-probe.exe\" --handler \"Z:${T}/got\" \"%1\"" /f >/dev/null 2>&1
"$WINESERVER" -w

for n in 50 1500 10000; do
    rm -f "$T/got"
    out=$(timeout -s KILL 120 "$WINE" "$T/longurl-probe.exe" "$n" 2>&1 | tr -d '\r')
    len=$(printf '%s\n' "$out" | sed -n 's/^url //p')
    rc=$(printf '%s\n' "$out" | sed -n 's/^rc //p')
    "$WINESERVER" -w
    got=$( { tr -d '\r\n' < "$T/got"; } 2>/dev/null)
    if [ -z "$rc" ]; then fail "a $len-character URL: ShellExecute crashed"
    elif [ "$got" = "$len" ]; then pass "a $len-character URL reaches its handler whole (rc $rc)"
    else fail "a $len-character URL: rc $rc, the handler got ${got:-nothing}"
    fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
