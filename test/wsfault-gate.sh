#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# webservices faults (6077, N-WEBSERVICES): WsCreateFaultFromError, WsSetFaultErrorDetail and
# WsSendFaultMessageForError were stubs, so a WWSAPI service could not answer a failed operation with a fault.
#  - WsCreateFaultFromError: the error's own fault as it is, else a generic one whose reason is the error's
#    strings (oldest first) or the failure code's text, in the error's language; minimal disclosure gives a
#    generic text only; a success code, no error, no heap, a bad disclosure: E_INVALIDARG;
#  - WsSetFaultErrorDetail writes the detail element into the error's fault (WsGetFaultErrorDetail reads it
#    back) and sets the action;
#  - WsSendFaultMessageForError answers a request on an HTTP reply channel with that fault (SOAP 1.2 and 1.1):
#    the client's WsRequestReply gets WS_E_ENDPOINT_FAULT_RECEIVED with the reason, the action and the detail.
#  - fixed on the way: a fault reply made WsRequestReply return WS_E_INVALID_FORMAT instead of
#    WS_E_ENDPOINT_FAULT_RECEIVED (the receive loop swallowed it); a SOAP 1.2 fault code written in a SOAP 1.1
#    envelope failed (standard codes are mapped now, Sender/Receiver <-> Client/Server, other namespaces
#    declared); the detail is written as the version's detail element; WsGetFaultErrorDetail reads a SOAP 1.2
#    Detail too.
# Runs in a network namespace of its own (unshare -rn, loopback only).
#   WINE=/opt/wine-sg/bin/wine test/wsfault-gate.sh   (mutants SG_MUTANT_WSFAULT_NO_STRINGS,
#   SG_MUTANT_WSFAULT_NO_DETAIL)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" unshare ip; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged user and network namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wsfault.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wsfault-probe.exe" "$HERE/wsfault-probe.c" \
    -idirafter "${WINE_INCLUDE:-/opt/wine-sg/include/wine/windows}" -lwebservices -lws2_32 \
    || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
cat > "$T/inner.sh" <<INNER
set -u
cd "$T"
ip link set lo up
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wsfault-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" wsfault-probe.exe > "$T/out" 2>"\${WSF_LOG:-/dev/null}" </dev/null)
"$WINESERVER" -k 2>/dev/null
INNER
unshare -rn sh "$T/inner.sh"
tr -d '\r' < "$T/out" 2>/dev/null | sed 's/^/      /'
grep -q '^RESULT: PASS' "$T/out" 2>/dev/null && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
