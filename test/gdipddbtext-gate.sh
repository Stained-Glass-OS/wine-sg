#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GDI+ text on a 32-bit compatible bitmap keeps its alpha, as on a 32-bit DIB section (patches/sg/6122):
# SumatraPDF's tab labels, drawn into such a buffer shown in the DWM frame, were transparent.
# test/gdipddbtext-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/gdipddbtext-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (gdiplus graphics.c): SG_MUTANT_DDB_TEXT_BY_GDI.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-gdipddbtext.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/gdipddbtext-probe.exe" "$HERE/gdipddbtext-probe.c" -lgdiplus -lgdi32 || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/gdipddbtext-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" gdipddbtext-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
printf '%s\n' "$out" | grep -q '^screen bpp 32' || { echo "SKIP: the screen is not 32-bit"; exit 77; }
for h in -1 1 4 5; do
    ddb=$(printf '%s\n' "$out" | sed -n "s/^DDB hint $h: .*alpha 255: \([0-9]*\)$/\1/p")
    dib=$(printf '%s\n' "$out" | sed -n "s/^DIB hint $h: .*alpha 255: \([0-9]*\)$/\1/p")
    if [ -n "$ddb" ] && [ -n "$dib" ] && [ "$dib" -gt 20 ] && [ "$ddb" -ge $((dib * 3 / 4)) ]; then
        printf 'PASS  hint %s: text on a 32-bit compatible bitmap is opaque (%s pixels at alpha 255; DIB %s)\n' "$h" "$ddb" "$dib"
    else
        printf 'FAIL  hint %s: text on a 32-bit compatible bitmap left transparent (%s opaque; DIB %s)\n' "$h" "${ddb:-?}" "${dib:-?}"; RC=1
    fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
