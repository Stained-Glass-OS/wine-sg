#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# LPC ports (6076, K-LPC): NtCreatePort, NtConnectPort and the rest returned STATUS_NOT_IMPLEMENTED. They are
# wineserver port objects now (connection, client and server communication ports, as Microsoft's NT
# Debugging blog describes LPC), messages in the 64-bit layout for 32-bit callers too:
#  - NtCreatePort; a second port of the same name: STATUS_OBJECT_NAME_COLLISION; NtReplyWaitReceivePortEx
#    with nothing queued times out;
#  - NtConnectPort sends the connection information; the server receives LPC_CONNECTION_REQUEST,
#    NtAcceptConnectPort answers with its own information (the client gets it back) and a port context,
#    NtCompleteConnectPort lets the client go; the largest message is reported; a refused connection:
#    STATUS_PORT_CONNECTION_REFUSED; no such port: STATUS_OBJECT_NAME_NOT_FOUND;
#  - NtRequestPort (LPC_DATAGRAM, the caller's message untouched), NtRequestWaitReplyPort and NtReplyPort
#    (LPC_REPLY, the sender's client id, the port context in NtReplyWaitReceivePort); a reply to an id
#    nobody sent: STATUS_REPLY_MESSAGE_MISMATCH; data over the port's largest message:
#    STATUS_PORT_MESSAGE_TOO_LONG; a header that does not hold its data: STATUS_INVALID_PARAMETER;
#  - NtSecureConnectPort checks the server's user: a different SID gives STATUS_SERVER_SID_MISMATCH;
#  - NtRegisterThreadTerminatePort: LPC_CLIENT_DIED when the thread ends; a client in another process,
#    64-bit and 32-bit, connects, asks and ends: LPC_CLIENT_DIED and LPC_PORT_CLOSED;
#  - the server closing the server port under a waiting client: STATUS_PORT_DISCONNECTED; a closed
#    connection port cannot be connected to.
#   WINE=/opt/wine-sg/bin/wine test/lpcport-gate.sh   (mutants SG_MUTANT_LPC_NO_CONTEXT,
#   SG_MUTANT_LPC_KEEP_WAITING, SG_MUTANT_LPC_NO_SID_CHECK)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-lpcport.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/lpcport-probe.exe" "$HERE/lpcport-probe.c" -lntdll || { fail "the probe did not build"; exit 1; }
"$MINGW32" -O2 -o "$T/lpcport-probe32.exe" "$HERE/lpcport-probe.c" -lntdll || { fail "the 32-bit probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/lpcport-probe.exe" "$T/lpcport-probe32.exe" "$C/"
(cd "$C" && timeout -s KILL 120 "$WINE" lpcport-probe.exe > "$T/out" 2>"${LPC_LOG:-/dev/null}" </dev/null)
out=$(tr -d '\r' <"$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Exports 1 "NtRequestPort, NtReplyPort and NtReplyWaitReceivePortEx are exported"
expect Create "0x00000000 0xc0000035" "NtCreatePort; the same name again: STATUS_OBJECT_NAME_COLLISION"
expect Timeout 0x00000102 "NtReplyWaitReceivePortEx with nothing queued: STATUS_TIMEOUT"
expect Connect "0x00000000 welcome 328" "NtConnectPort: accepted, the server's connection information back, the largest message"
expect Refused "0xc0000041 1" "a refused connection: STATUS_PORT_CONNECTION_REFUSED"
expect Missing 0xc0000034 "no such port: STATUS_OBJECT_NAME_NOT_FOUND"
expect Datagram "0x00000000 0 1 dg1 0" "NtRequestPort: an LPC_DATAGRAM with the port context; the caller's message untouched"
expect Request "0x00000000 2 18 1 reply:ping:4660:1" "NtRequestWaitReplyPort: LPC_REPLY with the server's data (context 0x1234, sender's client id)"
expect Mismatch "0x00000000 0xc000021f reply:twice:4660:1" "a reply to an id nobody sent: STATUS_REPLY_MESSAGE_MISMATCH"
expect BadMessages "0xc000002f 0xc000000d" "too much data: STATUS_PORT_MESSAGE_TOO_LONG; a header short of its data: STATUS_INVALID_PARAMETER"
expect SecureConnect "0x00000000 0xc00002a0" "NtSecureConnectPort: the server's user passes, another SID: STATUS_SERVER_SID_MISMATCH"
expect ClientDied "1 1" "NtRegisterThreadTerminatePort: LPC_CLIENT_DIED with the thread's id when it ends"
expect Disconnected 0xc0000037 "the server port closed under a waiting client: STATUS_PORT_DISCONNECTED"
expect Child0 "0 1 1" "a 64-bit client in another process: request answered, LPC_CLIENT_DIED and LPC_PORT_CLOSED at its end"
expect Child1 "0 1 1" "a 32-bit client in another process (64-bit message layout) likewise"
expect Quit "0x00000000 bye" "the last request answered"
expect ServerEnd 1 "the server thread ended"
expect AfterClose 0xc0000034 "a closed connection port cannot be connected to"
expect done 1 "the probe ran to the end"
printf '%s\n' "$out" | grep -q "^Unexpected=" && fail "the server got a message of an unexpected type" || pass "no unexpected message types"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
