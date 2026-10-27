#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Wait completion packets (patches/sg/6125): a packet ties a waitable object to an I/O completion
# port -- when the object is signalled, a completion with the packet's key, APC context and
# information is queued (at once if it is signalled already); cancelled before that, nothing is.
# The Windows App SDK's CoreMessagingXP (WinUI 3's dispatcher) uses them: App Installer died at
# NtCreateWaitCompletionPacket. test/waitpacket-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/waitpacket-gate.sh
# Mutant (dlls/ntdll/threadpool.c): SG_MUTANT_WAIT_PACKET_NOT_QUEUED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-waitpacket.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/waitpacket-probe.exe" "$HERE/waitpacket-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/waitpacket-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" waitpacket-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^exports=1$' "ntdll exports the three calls"
chk '^create=0x00000000$' "a packet is created"
chk '^dup=1$' "it is a handle (duplicated and closed)"
chk '^assoc=0x00000000 already=0$' "associated with an unsignalled object: not signalled already"
chk '^early=0$' "nothing is queued before the object is signalled"
chk '^queued=1 key=0x1234 apc=0*5678 info=42$' "once it is, the completion arrives with the packet's key, APC context and information"
chk '^consumed=1$' "and the wait was satisfied (an auto-reset event is reset)"
chk '^assoc2=0x00000000 already2=1$' "an object signalled already: reported so"
chk '^queued2=1 key2=0xa info2=7$' "and the completion is queued at once"
chk '^cancel=0x00000000$' "cancelled before the object is signalled"
chk '^after_cancel=0$' "nothing is queued after the cancel"
chk '^still_set=1$' "and the object is left signalled"
chk '^cancel_idle=0xc0000120$' "cancelling a packet with nothing to cancel: STATUS_CANCELLED"
chk '^close=1$' "the packet's handle closes"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
