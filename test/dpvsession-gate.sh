#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dpvoice (6064, C3): DirectPlay Voice's server and client answered E_NOTIMPL. They now keep a voice session's
# state on a DirectPlay 8 transport (hosted on localhost here), answering as the dvoice.h codes and Wine's
# todo_wine tests describe; no voice is carried, our transport having no voice channel:
#  - Initialize: no transport DVERR_NOTRANSPORT (Wine's test), uninitialized transport DVERR_TRANSPORTNOTINIT, one in
#    no session DVERR_TRANSPORTNOSESSION, a non-DirectPlay object DVERR_NOTRANSPORT, a bad mask, a mask without a
#    handler (DVERR_NOCALLBACK), twice (DVERR_INITIALIZED); on a hosting server it succeeds (Wine's test);
#  - StartSession (Wine's test: a mixing session with the default compression) and its checks, GetSessionDesc,
#    SetSessionDesc (buffering only), GetCaps, StopSession (DVERR_NOTHOSTING twice), transmit targets only where
#    the server controls them (DVERR_NOTALLOWED otherwise), DVERR_BUFFERTOOSMALL with the count;
#  - the client: Connect finds no voice session (DVERR_NOVOICESESSION; asynchronously DVERR_PENDING and a
#    DVMSGID_CONNECTRESULT), the connected calls answer DVERR_NOTCONNECTED; CheckAudioSetup's query DVERR_RUNSETUP.
#   WINE=/opt/wine-sg/bin/wine test/dpvsession-gate.sh   (mutants SG_MUTANT_DPV_ANY_COMPRESSION,
#   SG_MUTANT_DPV_STOP_UNHOSTED, SG_MUTANT_DPV_CONNECT_OK)
#   WINE_INCLUDE=<wine source include dir> when the installed headers lack dvoice.h
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dpvsession.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
inc="-idirafter ${WINE_INCLUDE:-/nonexistent} -idirafter $(dirname "$WINE")/include/wine/windows -idirafter /opt/wine-sg/include/wine/windows"
# shellcheck disable=SC2086  # the include list is words
TMPDIR=/var/tmp "$MINGW" -O2 $inc -o "$T/dpvsession-probe.exe" "$HERE/dpvsession-probe.c" -lole32 -luuid -ldxguid 2>"$T/cc.err" \
    || { cat "$T/cc.err"; fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/dpvsession-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" dpvsession-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect objects 1 "the DirectPlay 8 and Voice objects are made"
expect InitNull 0x80150188 "Initialize without a transport: DVERR_NOTRANSPORT"
expect InitUninit 0x8015018a "on an uninitialized transport: DVERR_TRANSPORTNOTINIT"
expect InitNoSession 0x8015018b "on one in no session: DVERR_TRANSPORTNOSESSION"
expect InitNotDplay 0x80150188 "on what is not a DirectPlay object: DVERR_NOTRANSPORT"
expect StartUninit 0x80150169 "StartSession before Initialize: DVERR_NOTINITIALIZED"
expect Host 0 "the transport hosts a session on localhost"
expect InitBadMask 0x80070057 "a message mask with an unknown message: DVERR_INVALIDPARAM"
expect InitMaskNoHandler 0x80150189 "a mask without a handler: DVERR_NOCALLBACK"
expect Init 0 "Initialize on the hosting server"
expect InitAgain 0x80150187 "twice: DVERR_INITIALIZED"
expect GetDescNotHosting 0x8015017b "GetSessionDesc before a session: DVERR_NOTHOSTING"
expect StartBadType 0x80070057 "an unknown session type"
expect StartBadCompression 0x80150172 "an unknown compression: DVERR_COMPRESSIONNOTSUPPORTED"
expect StartBadSize 0x80070057 "a bad description size"
expect Start 0 "StartSession: a mixing session with the default compression"
expect StartAgain 0x8015017a "twice: DVERR_HOSTING"
expect GetDesc "0 2 1" "GetSessionDesc gives it back"
expect SetDesc "0 50 2" "SetSessionDesc changes the buffering, not the type"
expect Caps 0 "GetCaps"
expect TargetsNotAllowed 0x8015016f "transmit targets where the server does not control them: DVERR_NOTALLOWED"
expect SetMaskOk 0 "SetNotifyMask"
expect Stop 0 "StopSession"
expect StopAgain 0x8015017b "twice: DVERR_NOTHOSTING"
expect StartControl 0 "a forwarding session the server controls the targets of"
expect SetTargets 0 "SetTransmitTargets"
expect GetTargetsSmall "0x8015001e 3" "GetTransmitTargets: DVERR_BUFFERTOOSMALL with the count"
expect GetTargets "0 3 7 8 9" "and the targets"
expect GetTargetsOther "0 0" "another source has none"
expect ClientNoSession 0x8015018b "a client on a transport in no session"
expect ServerOnClient 0x8015018b "a server on it too"
expect ConnectUninit 0x80150169 "Connect before Initialize: DVERR_NOTINITIALIZED"
expect ClientInit 0 "a client on the host's transport"
expect ConnectBadSize 0x80070057 "Connect with a bad configuration size"
expect ConnectSync 0x8015012e "Connect finds no voice session: DVERR_NOVOICESESSION"
expect ConnectAsync "0x150010 1 0x8015012e" "asynchronously: DVERR_PENDING, then DVMSGID_CONNECTRESULT with the result"
expect ClientNotConnected 0x8015016b "Disconnect: DVERR_NOTCONNECTED"
expect ClientCaps 0 "the client's GetCaps"
expect ClientUninitDesc 0x80150169 "an uninitialized client: DVERR_NOTINITIALIZED"
expect SetupQuery 0x80150183 "CheckAudioSetup's query: DVERR_RUNSETUP"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
