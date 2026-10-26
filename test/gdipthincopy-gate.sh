#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GDI+ thin lines with CompositingModeSourceCopy copy only the pixels they cover (patches/sg/6120):
# the second diagonal of a cross erased the first (PyCharm's close button). test/gdipthincopy-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/gdipthincopy-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (gdiplus graphics.c): SG_MUTANT_THINPATH_COPY_ALL.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-gdipthincopy.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/gdipthincopy-probe.exe" "$HERE/gdipthincopy-probe.c" -lgdiplus || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/gdipthincopy-probe.exe" "$WINEPREFIX/drive_c/"
RC=0
for mode in 1 0; do
    name=SourceCopy; [ $mode = 0 ] && name=SourceOver
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" gdipthincopy-probe.exe $mode 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | grep -v '^[A-Z]' | sed 's/^/      /'
    chk() { if printf '%s\n' "$out" | grep -qx "$1 1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
    chk DIAG "$name: the first diagonal of the cross is there"
    chk ANTI "$name: the second diagonal is there"
    chk BACK "$name: the rest is the black background"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
