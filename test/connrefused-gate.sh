#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A refused TCP connection takes Windows' time (patches/sg/6527): the server tries a connection the
# other side resets twice more (0.5 s, then 1 s later) before it fails, so a closed port refuses
# after about 1.5 s and a server that starts listening meanwhile is connected to (HandBrake's
# worker). Blocking connect, non-blocking connect + select, ConnectEx, a listener 0.3 s and 1.2 s
# late; an open port still connects at once (test/connrefused-probe.c). Loopback only, no display.
#   WINE=/opt/wine-sg/bin/wine test/connrefused-gate.sh
# Mutant: SG_MUTANT_SOCK_NO_RST_RETRY (server sock.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-connrefused.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/connrefused-probe.c" -lws2_32 -lmswsock || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check open_port   "an open port connects at once"
check blocking    "a blocking connect to a closed port: WSAECONNREFUSED after 1.2-4 s"
check nonblocking "non-blocking: select() reports the refusal (SO_ERROR) after 1.2-4 s"
check connectex   "ConnectEx: WSAECONNREFUSED after 1.2-4 s"
check late_300ms  "a server listening 0.3 s after the connect began is connected to"
check late_1200ms "and one listening 1.2 s after"
check done        "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
