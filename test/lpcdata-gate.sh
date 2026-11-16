#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# LPC request data (7809, K-LPC): NtReadRequestData and NtWriteRequestData were missing. A request may carry
# data information (VirtualRangesOffset: a count and { base, size } entries, 64-bit layout for 32-bit callers
# too) describing buffers of the sender's; while it waits for the reply, the process that received the
# request reads and writes them through the port:
#  - the receiver sees VirtualRangesOffset as sent; reading entry 0 gives the sender's buffer;
#  - reading or writing more than an entry's size, or an entry past the count: STATUS_INVALID_PARAMETER,
#    and nothing past the entry is read;
#  - writing entry 1 changes the sender's buffer (it finds the server's text there after the reply);
#  - another message id or another client id: STATUS_REPLY_MESSAGE_MISMATCH; once replied to, the request's
#    buffers are out of reach (STATUS_REPLY_MESSAGE_MISMATCH);
#  - for a 64-bit and a 32-bit sender in another process.
#   WINE=/opt/wine-sg/bin/wine test/lpcdata-gate.sh   (mutant SG_MUTANT_LPC_DATA_UNBOUNDED, server/port.c)
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
T=$(mktemp -d /var/tmp/sg-lpcdata.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/lpcdata-probe.exe" "$HERE/lpcdata-probe.c" -lntdll || { fail "the probe did not build"; exit 1; }
"$MINGW32" -O2 -o "$T/lpcdata-probe32.exe" "$HERE/lpcdata-probe.c" -lntdll || { fail "the 32-bit probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/lpcdata-probe.exe" "$T/lpcdata-probe32.exe" "$C/"
(cd "$C" && timeout -s KILL 120 "$WINE" lpcdata-probe.exe > "$T/out" 2>"${LPC_LOG:-/dev/null}" </dev/null)
out=$(tr -d '\r' <"$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Exports 1 "NtReadRequestData and NtWriteRequestData are exported"
for w in W64 W32; do
    expect $w.Request "0x00000000 1 56" "$w: the request arrives with its VirtualRangesOffset"
    expect $w.Read "0x00000000 24 client-buffer-A" "$w: NtReadRequestData reads the sender's buffer (entry 0)"
    expect $w.Over "0xc000000d 0" "$w: more than the entry's size: STATUS_INVALID_PARAMETER, nothing past it read"
    expect $w.Index 0xc000000d "$w: an entry past the count: STATUS_INVALID_PARAMETER"
    expect $w.Write "0x00000000 18" "$w: NtWriteRequestData writes the sender's buffer (entry 1)"
    expect $w.WriteOver 0xc000000d "$w: writing more than the entry's size: STATUS_INVALID_PARAMETER"
    expect $w.OtherId 0xc000021f "$w: another message id: STATUS_REPLY_MESSAGE_MISMATCH"
    expect $w.OtherClient 0xc000021f "$w: another client id: STATUS_REPLY_MESSAGE_MISMATCH"
    expect $w.After 0xc000021f "$w: once replied to, out of reach"
    expect $w.Child 0 "$w: the sender got the reply and finds the server's text in its buffer"
done
expect Done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
