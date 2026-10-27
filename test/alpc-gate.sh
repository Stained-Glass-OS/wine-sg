#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ALPC ports within one process (patches/sg/6126): the Windows App SDK's CoreMessagingXP (WinUI 3)
# creates its registrar port and connects to it from the same process; App Installer died at
# NtAlpcCreatePort. test/alpc-probe.c: connection request and accept (with a port context),
# synchronous request and reply, release, datagram, disconnect, message attributes.
#   WINE=/opt/wine-sg/bin/wine test/alpc-gate.sh
# Mutant (dlls/ntdll/alpc.c): SG_MUTANT_ALPC_REPLY_LOST.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-alpc.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/alpc-probe.exe" "$HERE/alpc-probe.c" -lntdll || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/alpc-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" alpc-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^exports=1$' "ntdll exports the ALPC calls and attribute helpers"
chk '^attr_size=72$' "attributes security+context+work-on-behalf need 72 bytes (header 8, 24, 32, 8)"
chk '^attr_init=0x00000000$' "the attribute buffer is initialised"
chk '^attr_ctx_offset=32$' "the context attribute follows the security one"
chk '^attr_absent=1$' "an attribute not allocated is not there"
chk '^create=0x00000000$' "a named server port is created"
chk '^create_again=0xc0000035$' "a second port of that name: STATUS_OBJECT_NAME_COLLISION"
chk '^srv_conn=hello$' "the server receives the connection request with its data"
chk '^srv_accept=0x00000000$' "and accepts it"
chk '^connect=0x00000000$' "the client is connected"
chk '^srv_req=ping ctx=0*c0ffee valid=1$' "the server receives the request, with the port context it accepted with"
chk '^srv_release=0x00000000$' "a release that names no request is accepted (nothing is sent)"
chk '^srv_reply=0x00000000$' "the reply is sent (with the release flag)"
chk '^request=0x00000000 reply_type=2 reply=pong$' "the client's synchronous request gets that reply"
chk '^srv_dgram=note$' "a datagram arrives"
chk '^srv_closed=1$' "the client's disconnect reaches the server"
chk '^connect_missing=0xc0000034$' "a port that does not exist: STATUS_OBJECT_NAME_NOT_FOUND"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
