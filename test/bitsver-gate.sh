#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The versioned BITS manager classes (patches/sg/6223): BackgroundCopyManager1_5, 2_0, 2_5 and 3_0
# are served by the same manager as BackgroundCopyManager (OneDrive asks for 3_0 and got "class not
# registered"); a job made through one answers IBackgroundCopyJob3. test/bitsver-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/bitsver-gate.sh
# Mutant: SG_MUTANT_BITS_VERSIONED_CLASSES (dlls/qmgr/service.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bitsver.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/bitsver-probe.exe" "$HERE/bitsver-probe.c" -lole32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/bitsver-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" bitsver-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^v1_0=00000000,00000000,00000000$' "BackgroundCopyManager (1.0) as before"
chk '^v1_5=00000000,00000000,00000000$' "BackgroundCopyManager1_5"
chk '^v2_0=00000000,00000000,00000000$' "BackgroundCopyManager2_0"
chk '^v2_5=00000000,00000000,00000000$' "BackgroundCopyManager2_5"
chk '^v3_0=00000000,00000000,00000000$' "BackgroundCopyManager3_0 (OneDrive's)"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
